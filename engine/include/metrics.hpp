#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <map>

namespace sfcw {

class Timer {
public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        end_time_ = std::chrono::high_resolution_clock::now();
    }

    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(end_time_ - start_time_).count();
    }

    double elapsed_seconds() const {
        return std::chrono::duration<double>(end_time_ - start_time_).count();
    }

    int64_t elapsed_microseconds() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(end_time_ - start_time_).count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time_;
};

struct FileAnalysis {
    uint64_t total_bytes = 0;
    uint32_t distinct_symbols = 0;
    double entropy = 0.0; // Shannon entropy in bits per symbol (0 to 8)
    uint64_t total_runs = 0;
    double avg_run_length = 0.0;
    double repetition_ratio = 0.0; // percentage of bytes in runs >= 3
    uint64_t frequency[256] = {0};

    std::string recommended_algorithm;
    std::string recommendation_reason;

    std::string to_json() const;
};

FileAnalysis analyze_bytes(const uint8_t* data, size_t size);

struct AlgorithmResult {
    std::string algorithm_name;
    std::string algorithm_slug;
    uint64_t original_size = 0;
    uint64_t compressed_size = 0;
    uint64_t payload_size = 0;
    uint64_t metadata_size = 0;
    double compression_ratio = 0.0; // original / compressed
    double space_saved_percent = 0.0;
    double compression_time_ms = 0.0;
    double decompression_time_ms = 0.0;
    double compression_throughput_mb_s = 0.0;
    double decompression_throughput_mb_s = 0.0;
    bool integrity_verified = false;
    uint32_t original_crc32 = 0;
    uint32_t restored_crc32 = 0;
    std::string theoretical_time_complexity;
    std::string theoretical_space_complexity;

    std::string to_json() const;
};

} // namespace sfcw
