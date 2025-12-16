#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

struct Job {
    std::string input_path;
    std::string benchmark_set; // Parent folder name
    std::string filename;      // e.g. "instance.cnf"
    std::string solver_name;
    std::string cmd_template;
    bool no_pinning;
};

namespace Scanner {
    inline std::vector<Job> scan(const std::vector<std::string>& inputs, const std::vector<SolverConfig>& solvers) {
        std::vector<Job> jobs;
        
        for (const auto& input : inputs) {
            if (!fs::exists(input)) {
                std::cerr << "Warning: Input path does not exist: " << input << std::endl;
                continue;
            }

            if (fs::is_directory(input)) {
                for (const auto& entry : fs::recursive_directory_iterator(input)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".cnf") {
                        std::string set_name = entry.path().parent_path().filename().string();
                        
                        for (const auto& solver : solvers) {
                            Job j;
                            j.input_path = entry.path().string();
                            j.benchmark_set = set_name;
                            j.filename = entry.path().filename().string();
                            j.solver_name = solver.name;
                            j.cmd_template = solver.command_template;
                            j.no_pinning = solver.no_pinning;
                            jobs.push_back(j);
                        }
                    }
                }
            } else if (fs::is_regular_file(input)) {
                 // Handle single file
                 // For single file, parent dir is the set name
                 std::string set_name = fs::path(input).parent_path().filename().string();
                 for (const auto& solver : solvers) {
                    Job j;
                    j.input_path = input;
                    j.benchmark_set = set_name;
                    j.filename = fs::path(input).filename().string();
                    j.solver_name = solver.name;
                    j.cmd_template = solver.command_template;
                    j.no_pinning = solver.no_pinning;
                    jobs.push_back(j);
                }
            }
        }
        return jobs;
    }
}
