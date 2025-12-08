#ifndef PROFILER_H
#define PROFILER_H

#include <string>
#include <fstream>
#include <sstream>

namespace vfs {
namespace utils {

/**
 * @brief Simple system resource profiler
 */
class Profiler {
public:
    struct MemoryInfo {
        size_t virtual_memory_kb;
        size_t resident_memory_kb;
        size_t shared_memory_kb;
    };

    struct CPUInfo {
        double cpu_usage_percent;
        size_t num_threads;
    };

    /**
     * @brief Get current memory usage
     */
    static MemoryInfo getMemoryUsage();

    /**
     * @brief Get current CPU usage
     */
    static CPUInfo getCPUUsage();

    /**
     * @brief Print formatted resource usage
     */
    static void printResourceUsage();

private:
    static long parseLine(const std::string& line);
};

} // namespace utils
} // namespace vfs

#endif // PROFILER_H
