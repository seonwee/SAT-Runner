#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <future>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <atomic>

#include "CLI11.hpp"
#include "Config.hpp"
#include "Utils.hpp"
#include "ResourceManager.hpp"
#include "Scanner.hpp"

// Platform check
#ifndef __linux__
#error "This tool is designed for Linux only due to dependency on taskset and RunSolver."
#endif

namespace fs = std::filesystem;

std::mutex csv_mtx;
std::atomic<int> completed_jobs(0);

void run_job(Job job, AppConfig config, ResourceManager& resource_manager, int job_id) {
    int core_id = -1;
    
    // 1. Resource Allocation
    if (!job.no_pinning) {
        core_id = resource_manager.checkoutCore();
    }

    // 2. Prepare Directories
    // Output/Set/Solver/
    fs::path out_dir = fs::path(config.output_root) / job.benchmark_set / job.solver_name;
    try {
        fs::create_directories(out_dir);
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory: " << e.what() << std::endl;
        if (core_id != -1) resource_manager.returnCore(core_id);
        return;
    }

    std::string base_name = fs::path(job.filename).stem().string(); // remove extension
    fs::path log_path = out_dir / (base_name + ".log");
    fs::path err_path = out_dir / (base_name + ".err");
    fs::path watcher_path = out_dir / (base_name + ".watcher");

    // 3. Command Construction
    // Template replacement
    std::string cmd = job.cmd_template;
    
    // Simple replacement helper
    auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    };

    replace_all(cmd, "{input}", job.input_path);
    replace_all(cmd, "{timeout}", std::to_string((int)config.timeout));
    
    // Wrap with runsolver
    // runsolver --timestamp -C {timeout} -M {mem} -w {watcher} -o {log} -e {err} -- {cmd}
    std::string runsolver_cmd = "runsolver --timestamp -C " + std::to_string((int)config.timeout) + 
                                " -M " + std::to_string(config.mem_limit_mb) + 
                                " -w " + watcher_path.string() + 
                                " -o " + log_path.string() +
                                " -e " + err_path.string() + 
                                " -- " + cmd;

    // Wrap with taskset if pinned
    std::string final_cmd;
    if (core_id != -1) {
        final_cmd = "taskset -c " + std::to_string(core_id) + " " + runsolver_cmd;
    } else {
        final_cmd = runsolver_cmd;
    }

    // 4. Execution
    int ret = std::system(final_cmd.c_str());

    // 5. Logging to Index
    {
        std::lock_guard<std::mutex> lock(csv_mtx);
        std::ofstream csv(fs::path(config.output_root) / "index.csv", std::ios::app);
        if (csv.is_open()) {
            csv << job.benchmark_set << "," << job.solver_name << "," << job.filename << "," 
                << (core_id != -1 ? std::to_string(core_id) : "N/A") << "," << ret << "\n";
        }
    }

    // 6. Resource Return
    if (core_id != -1) {
        resource_manager.returnCore(core_id);
    }

    completed_jobs++;
}

int main(int argc, char** argv) {
    CLI::App app{"SAT Runner - High Performance Benchmarking Tool"};

    AppConfig config;
    std::string cpu_list_str;
    std::string config_path;

    app.add_option("-i,--input", config.input_paths, "Input files or directories")->required();
    app.add_option("-o,--output", config.output_root, "Output root directory")->required();
    app.add_option("-C,--cores", cpu_list_str, "Allowed CPU cores (e.g., 0-3,5). Auto-detected if omitted.");
    app.add_option("-j,--jobs", config.max_jobs, "Max concurrent jobs (limited by cores)");
    app.add_option("-c,--config", config_path, "Solvers config JSON")->required();
    app.add_option("-t,--timeout", config.timeout, "Timeout in seconds")->default_val(3600);
    app.add_option("-m,--mem-limit", config.mem_limit_mb, "Memory limit in MB")->default_val(16384);

    CLI11_PARSE(app, argc, argv);

    // 1. Parse CPU Cores
    if (!cpu_list_str.empty()) {
        config.allowed_cores = Utils::parseCpuList(cpu_list_str);
    } else {
        std::cout << "Auto-detecting physical cores..." << std::endl;
        config.allowed_cores = Utils::detectPhysicalCores();
    }

    if (config.allowed_cores.empty()) {
        std::cerr << "Error: No valid cores specified or detected." << std::endl;
        return 1;
    }
    
    std::cout << "Detected/Allowed Cores: " << config.allowed_cores.size() << " [";
    for(auto c : config.allowed_cores) std::cout << c << " ";
    std::cout << "]" << std::endl;

    // 2. Load Solvers
    config.solvers = loadSolvers(config_path);
    if (config.solvers.empty()) {
        std::cerr << "Error: No solvers loaded from config." << std::endl;
        return 1;
    }

    // 3. Scan Jobs
    std::vector<Job> jobs = Scanner::scan(config.input_paths, config.solvers);
    if (jobs.empty()) {
        std::cerr << "Warning: No jobs generated." << std::endl;
        return 0;
    }
    std::cout << "Total Jobs: " << jobs.size() << std::endl;

    // 4. Initialize Resource Manager
    ResourceManager rm(config.allowed_cores);

    // 5. Initialize Index CSV
    fs::create_directories(config.output_root);
    {
        std::ofstream csv(fs::path(config.output_root) / "index.csv");
        csv << "Benchmark_Set,Solver,Instance,CPU_Core_ID,Exit_Code\n";
    }

    // 6. Concurrency Logic
    // Limit concurrency by min(user_jobs, available_cores)
    // Actually, the Resource Manager blocks if no cores are available.
    // However, we shouldn't spawn more threads than max_jobs regardless of cores,
    // or if max_jobs > cores, the threads will just block waiting for cores.
    // The PRD says: "Concurrency = min(jobs, available cores)"
    
    int concurrency = std::min((size_t)config.max_jobs, config.allowed_cores.size());
    if (concurrency < 1) concurrency = 1;
    
    std::cout << "Concurrency Level: " << concurrency << std::endl;

    // We can use a simple semaphore-like approach or a fixed thread pool.
    // Since jobs > concurrency, a thread pool is best.
    // Here I'll use a simple "launcher" loop that keeps 'concurrency' futures active.
    
    std::vector<std::future<void>> futures;
    int next_job_idx = 0;

    // Helper to print progress
    auto print_progress = [&]() {
        std::cout << "\rProgress: " << completed_jobs << " / " << jobs.size() 
                  << " | Active Threads: " << futures.size() << "    " << std::flush;
    };

    while (next_job_idx < jobs.size() || !futures.empty()) {
        // Clean up finished futures
        for (auto it = futures.begin(); it != futures.end(); ) {
            if (it->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                it = futures.erase(it);
                print_progress();
            } else {
                ++it;
            }
        }

        // Spawn new jobs if slot available
        while (futures.size() < concurrency && next_job_idx < jobs.size()) {
            futures.push_back(std::async(std::launch::async, run_job, 
                                         jobs[next_job_idx], config, std::ref(rm), next_job_idx));
            next_job_idx++;
        }
        
        print_progress();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nAll jobs completed." << std::endl;
    return 0;
}
