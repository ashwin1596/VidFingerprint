#include "utils/profiler.h"
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <algorithm>

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

void Profiler::printSystemInfo() {
    std::cout << "\n=== System Information ===" << std::endl;
    
    // CPU info
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    std::string cpu_model;
    int cpu_cores = 0;
    
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") == 0) {
            size_t colon = line.find(':');
            if (colon != std::string::npos && cpu_model.empty()) {
                cpu_model = line.substr(colon + 2);
            }
        }
        if (line.find("processor") == 0) {
            cpu_cores++;
        }
    }
    
    std::cout << "CPU: " << cpu_model << std::endl;
    std::cout << "Cores: " << cpu_cores << std::endl;
    
    // Memory info
    std::ifstream meminfo("/proc/meminfo");
    long total_mem_kb = 0;
    
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            total_mem_kb = parseLine(line);
            break;
        }
    }
    
    std::cout << "RAM: " << std::fixed << std::setprecision(1) 
              << (total_mem_kb / 1024.0 / 1024.0) << " GB" << std::endl;
    
    // OS info
    std::ifstream osinfo("/etc/os-release");
    std::string os_name;
    
    while (std::getline(osinfo, line)) {
        if (line.find("PRETTY_NAME=") == 0) {
            os_name = line.substr(13);
            // Remove quotes
            os_name.erase(remove(os_name.begin(), os_name.end(), '"'), os_name.end());
            break;
        }
    }
    
    if (!os_name.empty()) {
        std::cout << "OS: " << os_name << std::endl;
    }
    
    // Compiler info
    std::cout << "Compiler: ";
#if defined(__clang__)
    std::cout << "Clang " << __clang_major__ << "." << __clang_minor__;
#elif defined(__GNUC__)
    std::cout << "GCC " << __GNUC__ << "." << __GNUC_MINOR__;
#else
    std::cout << "Unknown";
#endif
    std::cout << std::endl;
    
    std::cout << "Build: ";
#ifdef NDEBUG
    std::cout << "Release";
#else
    std::cout << "Debug";
#endif
    std::cout << std::endl;
    
    std::cout << std::endl;
}

} // namespace utils
} // namespace vfs
