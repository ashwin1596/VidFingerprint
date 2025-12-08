#include "utils/profiler.h"
#include <iostream>
#include <iomanip>
#include <unistd.h>

namespace vfs {
namespace utils {

long Profiler::parseLine(const std::string& line) {
    std::istringstream iss(line);
    std::string key;
    long value;
    std::string unit;
    
    iss >> key >> value >> unit;
    return value;
}

Profiler::MemoryInfo Profiler::getMemoryUsage() {
    MemoryInfo info = {0, 0, 0};
    
    std::ifstream status_file("/proc/self/status");
    std::string line;
    
    while (std::getline(status_file, line)) {
        if (line.find("VmSize:") == 0) {
            info.virtual_memory_kb = parseLine(line);
        } else if (line.find("VmRSS:") == 0) {
            info.resident_memory_kb = parseLine(line);
        } else if (line.find("RssFile:") == 0) {
            info.shared_memory_kb = parseLine(line);
        }
    }
    
    return info;
}

Profiler::CPUInfo Profiler::getCPUUsage() {
    CPUInfo info = {0.0, 0};
    
    // Get number of threads
    std::ifstream status_file("/proc/self/status");
    std::string line;
    
    while (std::getline(status_file, line)) {
        if (line.find("Threads:") == 0) {
            std::istringstream iss(line);
            std::string key;
            iss >> key >> info.num_threads;
            break;
        }
    }
    
    // Simple CPU usage calculation
    // In a real profiler, you'd sample /proc/self/stat over time
    info.cpu_usage_percent = 0.0; // Placeholder
    
    return info;
}

void Profiler::printResourceUsage() {
    auto mem = getMemoryUsage();
    auto cpu = getCPUUsage();
    
    std::cout << "\n=== Resource Usage ===" << std::endl;
    std::cout << "Memory:" << std::endl;
    std::cout << "  Virtual Memory: " << std::fixed << std::setprecision(2)
              << (mem.virtual_memory_kb / 1024.0) << " MB" << std::endl;
    std::cout << "  Resident Memory (RSS): " 
              << (mem.resident_memory_kb / 1024.0) << " MB" << std::endl;
    std::cout << "  Shared Memory: " 
              << (mem.shared_memory_kb / 1024.0) << " MB" << std::endl;
    std::cout << "\nThreads:" << std::endl;
    std::cout << "  Active Threads: " << cpu.num_threads << std::endl;
    std::cout << std::endl;
}

} // namespace utils
} // namespace vfs
