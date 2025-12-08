#include "monitoring/metrics.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace vfs {
namespace monitoring {

MetricsCollector::MetricsCollector() {}

void MetricsCollector::recordLatency(const std::string& operation, uint64_t latency_us) {
    std::lock_guard<std::mutex> lock(mutex_);
    latencies_[operation].push_back(latency_us);
}

void MetricsCollector::incrementCounter(const std::string& metric) {
    // Atomic counters don't need mutex
    counters_[metric].fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::recordGauge(const std::string& metric, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    gauges_[metric] = value;
}

MetricsCollector::LatencyStats 
MetricsCollector::getLatencyStats(const std::string& operation) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LatencyStats stats = {0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    auto it = latencies_.find(operation);
    if (it == latencies_.end() || it->second.empty()) {
        return stats;
    }

    auto data = it->second;
    std::sort(data.begin(), data.end());

    stats.count = data.size();
    stats.min_us = data.front();
    stats.max_us = data.back();
    stats.mean_us = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
    stats.p50_us = calculatePercentile(data, 0.50);
    stats.p95_us = calculatePercentile(data, 0.95);
    stats.p99_us = calculatePercentile(data, 0.99);

    return stats;
}

uint64_t MetricsCollector::getCounter(const std::string& metric) const {
    auto it = counters_.find(metric);
    if (it != counters_.end()) {
        return it->second.load(std::memory_order_relaxed);
    }
    return 0;
}

std::string MetricsCollector::getAllMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream ss;

    ss << "=== Metrics Report ===" << std::endl;
    ss << std::endl;

    // Counters
    ss << "Counters:" << std::endl;
    for (const auto& [name, counter] : counters_) {
        ss << "  " << name << ": " << counter.load(std::memory_order_relaxed) << std::endl;
    }
    ss << std::endl;

    // Gauges
    if (!gauges_.empty()) {
        ss << "Gauges:" << std::endl;
        for (const auto& [name, value] : gauges_) {
            ss << "  " << name << ": " << std::fixed << std::setprecision(2) 
               << value << std::endl;
        }
        ss << std::endl;
    }

    // Latencies
    if (!latencies_.empty()) {
        ss << "Latencies (microseconds):" << std::endl;
        for (const auto& [operation, _] : latencies_) {
            auto stats = getLatencyStats(operation);
            ss << "  " << operation << ":" << std::endl;
            ss << "    Count: " << stats.count << std::endl;
            ss << "    Mean:  " << std::fixed << std::setprecision(2) << stats.mean_us << " μs" << std::endl;
            ss << "    P50:   " << stats.p50_us << " μs" << std::endl;
            ss << "    P95:   " << stats.p95_us << " μs" << std::endl;
            ss << "    P99:   " << stats.p99_us << " μs" << std::endl;
            ss << "    Min:   " << stats.min_us << " μs" << std::endl;
            ss << "    Max:   " << stats.max_us << " μs" << std::endl;
        }
    }

    return ss.str();
}

void MetricsCollector::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    latencies_.clear();
    gauges_.clear();
    for (auto& [_, counter] : counters_) {
        counter.store(0, std::memory_order_relaxed);
    }
}

double MetricsCollector::calculatePercentile(
    const std::vector<uint64_t>& sorted_data, 
    double percentile) {
    
    if (sorted_data.empty()) {
        return 0.0;
    }

    double index = percentile * (sorted_data.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(index));
    size_t upper = static_cast<size_t>(std::ceil(index));

    if (lower == upper) {
        return sorted_data[lower];
    }

    double weight = index - lower;
    return sorted_data[lower] * (1.0 - weight) + sorted_data[upper] * weight;
}

} // namespace monitoring
} // namespace vfs
