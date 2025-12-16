#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include "json.hpp"

struct SolverConfig {
    std::string name;
    std::string command_template;
    bool no_pinning = false;
};

struct AppConfig {
    std::vector<std::string> input_paths;
    std::string output_root;
    std::vector<int> allowed_cores;
    int max_jobs = 1;
    double timeout = 3600.0;
    int mem_limit_mb = 16384;
    std::vector<SolverConfig> solvers;
};

inline std::vector<SolverConfig> loadSolvers(const std::string& config_path) {
    std::vector<SolverConfig> solvers;
    std::ifstream f(config_path);
    if (!f.is_open()) return solvers; // Or throw

    nlohmann::json j;
    f >> j;

    for (const auto& item : j) {
        SolverConfig s;
        s.name = item.value("name", "unknown");
        s.command_template = item.value("command", "");
        s.no_pinning = item.value("no_pinning", false);
        solvers.push_back(s);
    }
    return solvers;
}
