#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace Utils {

    // Helper to parse CPU string like "0-3,5" into vector [0, 1, 2, 3, 5]
    inline std::vector<int> parseCpuList(const std::string& cpu_str) {
        std::vector<int> cores;
        std::stringstream ss(cpu_str);
        std::string segment;

        while (std::getline(ss, segment, ',')) {
            size_t dash = segment.find('-');
            if (dash != std::string::npos) {
                int start = std::stoi(segment.substr(0, dash));
                int end = std::stoi(segment.substr(dash + 1));
                for (int i = start; i <= end; ++i) {
                    cores.push_back(i);
                }
            } else {
                if (!segment.empty()) {
                    cores.push_back(std::stoi(segment));
                }
            }
        }
        // Remove duplicates and sort
        std::sort(cores.begin(), cores.end());
        cores.erase(std::unique(cores.begin(), cores.end()), cores.end());
        return cores;
    }

    // Helper to detect available physical cores (skipping HT siblings)
    inline std::vector<int> detectPhysicalCores() {
        std::set<int> unique_cores;
        std::string base_path = "/sys/devices/system/cpu/";
        
        if (!std::filesystem::exists(base_path)) return {};

        for (const auto& entry : std::filesystem::directory_iterator(base_path)) {
            std::string dirname = entry.path().filename().string();
            // Check if directory name starts with "cpu" and is followed by a digit
            if (dirname.rfind("cpu", 0) == 0 && dirname.length() > 3 && std::isdigit(dirname[3])) {
                 std::string siblings_path = entry.path().string() + "/topology/thread_siblings_list";
                 std::ifstream f(siblings_path);
                 if (f.is_open()) {
                     std::string line;
                     std::getline(f, line);
                     // Format example: "0,16" or "0-3"
                     size_t sep = line.find_first_of(",-");
                     std::string first_core_str = (sep == std::string::npos) ? line : line.substr(0, sep);
                     try {
                        if (!first_core_str.empty()) {
                            unique_cores.insert(std::stoi(first_core_str));
                        }
                     } catch (...) {}
                 } else {
                     // Fallback: use the cpu ID itself if topology info is missing
                     try {
                         int id = std::stoi(dirname.substr(3));
                         unique_cores.insert(id);
                     } catch(...) {}
                 }
            }
        }
        
        std::vector<int> cores(unique_cores.begin(), unique_cores.end());
        return cores;
    }
}
