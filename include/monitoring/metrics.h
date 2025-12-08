#ifndef METRICS_H
#define METRICS_H

#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace vfs {
namespace monitoring {

/**
 * @brief Collects and tracks system metrics
 * 
 * Thread-safe metrics collection for monitoring system performance,
 * latency, throughput, and error rates.
 */
class MetricsCollector {
public:
    MetricsCollector();

    /**
     * @brief Record a latency measurement
     */
    void recordLatency(const std::string& operation, uint64_t latency_us);

    /**
     * @brief Increment a counter
     */
    void incrementCounter(const std::string& metric);

    /**
     * @brief Record a gauge value
     */
    void recordGauge(const std::string& metric, double value);

    /**
     * @brief Get latency statistics for an operation
     */
    struct LatencyStats {
        uint64_t count;
        double mean_us;
        double p50_us;
        double p95_us;
        double p99_us;
        double max_us;
        double min_us;
    };
    LatencyStats getLatencyStats(const std::string& operation) const;

    /**
     * @brief Get counter value
     */
    uint64_t getCounter(const std::string& metric) const;

    /**
     * @brief Get all metrics as string
     */
    std::string getAllMetrics() const;

    /**
     * @brief Reset all metrics
     */
    void reset();

    /**
     * @brief RAII helper for measuring operation latency
     */
    class Timer {
    public:
        Timer(MetricsCollector* collector, const std::string& operation)
            : collector_(collector)
            , operation_(operation)
            , start_(std::chrono::steady_clock::now()) {}

        ~Timer() {
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start_).count();
            collector_->recordLatency(operation_, duration);
        }

    private:
        MetricsCollector* collector_;
        std::string operation_;
        std::chrono::steady_clock::time_point start_;
    };

private:
    mutable std::mutex mutex_;
    
    // Latency tracking
    std::unordered_map<std::string, std::vector<uint64_t>> latencies_;
    
    // Counters
    std::unordered_map<std::string, std::atomic<uint64_t>> counters_;
    
    // Gauges
    std::unordered_map<std::string, double> gauges_;

    /**
     * @brief Calculate percentile from sorted data
     */
    static double calculatePercentile(const std::vector<uint64_t>& sorted_data, 
                                     double percentile);
};

} // namespace monitoring
} // namespace vfs

#endif // METRICS_H
