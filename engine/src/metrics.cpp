#include "metrics.hpp"
#include <sstream>
#include <iomanip>

namespace sfcw {

FileAnalysis analyze_bytes(const uint8_t* data, size_t size) {
    FileAnalysis analysis;
    analysis.total_bytes = size;

    if (size == 0) {
        analysis.recommended_algorithm = "huffman";
        analysis.recommendation_reason = "File is empty (0 bytes). Standard header container applies.";
        return analysis;
    }

    // 1. Calculate frequency distribution
    for (size_t i = 0; i < size; ++i) {
        analysis.frequency[data[i]]++;
    }

    for (int i = 0; i < 256; ++i) {
        if (analysis.frequency[i] > 0) {
            analysis.distinct_symbols++;
        }
    }

    // 2. Shannon Entropy
    double entropy = 0.0;
    double d_size = static_cast<double>(size);
    for (int i = 0; i < 256; ++i) {
        if (analysis.frequency[i] > 0) {
            double p = static_cast<double>(analysis.frequency[i]) / d_size;
            entropy -= p * (std::log(p) / std::log(2.0));
        }
    }
    analysis.entropy = entropy;

    // 3. Run analysis
    uint64_t runs = 0;
    uint64_t bytes_in_long_runs = 0;
    size_t i = 0;
    while (i < size) {
        runs++;
        size_t run_len = 1;
        while (i + 1 < size && data[i + 1] == data[i]) {
            run_len++;
            i++;
        }
        if (run_len >= 3) {
            bytes_in_long_runs += run_len;
        }
        i++;
    }

    analysis.total_runs = runs;
    analysis.avg_run_length = static_cast<double>(size) / static_cast<double>(runs);
    analysis.repetition_ratio = static_cast<double>(bytes_in_long_runs) / d_size;

    // 4. Transparent Heuristic Recommendation
    if (analysis.avg_run_length >= 2.5 || analysis.repetition_ratio >= 0.35) {
        analysis.recommended_algorithm = "rle";
        std::ostringstream ss;
        ss << "Heuristic: High consecutive byte repetition detected (" 
           << std::fixed << std::setprecision(1) << (analysis.repetition_ratio * 100.0) 
           << "% of bytes in runs >= 3, avg run length " << std::setprecision(2) << analysis.avg_run_length 
           << "). RLE encodes consecutive runs directly.";
        analysis.recommendation_reason = ss.str();
    } else if (analysis.entropy < 5.0 || analysis.distinct_symbols <= 48) {
        analysis.recommended_algorithm = "huffman";
        std::ostringstream ss;
        ss << "Heuristic: Low Shannon entropy (" 
           << std::fixed << std::setprecision(2) << analysis.entropy 
           << " bits/symbol vs max 8.0) and skewed distribution (" 
           << analysis.distinct_symbols << " active byte values). Huffman greedy prefix coding is optimal.";
        analysis.recommendation_reason = ss.str();
    } else {
        analysis.recommended_algorithm = "lzw";
        std::ostringstream ss;
        ss << "Heuristic: High symbol variety (" << analysis.distinct_symbols 
           << " active symbols, entropy " << std::fixed << std::setprecision(2) << analysis.entropy 
           << " bits/symbol). LZW dictionary building identifies repeating multi-byte phrases.";
        analysis.recommendation_reason = ss.str();
    }

    return analysis;
}

std::string FileAnalysis::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"totalBytes\": " << total_bytes << ",\n";
    ss << "  \"distinctSymbols\": " << distinct_symbols << ",\n";
    ss << "  \"entropy\": " << std::fixed << std::setprecision(4) << entropy << ",\n";
    ss << "  \"totalRuns\": " << total_runs << ",\n";
    ss << "  \"avgRunLength\": " << std::fixed << std::setprecision(2) << avg_run_length << ",\n";
    ss << "  \"repetitionRatio\": " << std::fixed << std::setprecision(4) << repetition_ratio << ",\n";
    ss << "  \"recommendedAlgorithm\": \"" << recommended_algorithm << "\",\n";
    ss << "  \"recommendationReason\": \"" << recommendation_reason << "\",\n";
    ss << "  \"topFrequencies\": [";
    
    // Sort top 10 frequencies
    std::vector<std::pair<uint8_t, uint64_t>> sorted_freqs;
    for (int i = 0; i < 256; ++i) {
        if (frequency[i] > 0) {
            sorted_freqs.push_back({static_cast<uint8_t>(i), frequency[i]});
        }
    }
    // Sort descending
    for (size_t a = 0; a < sorted_freqs.size(); ++a) {
        for (size_t b = a + 1; b < sorted_freqs.size(); ++b) {
            if (sorted_freqs[b].second > sorted_freqs[a].second) {
                std::swap(sorted_freqs[a], sorted_freqs[b]);
            }
        }
    }

    size_t limit = sorted_freqs.size() < 16 ? sorted_freqs.size() : 16;
    for (size_t i = 0; i < limit; ++i) {
        ss << "{\"byte\":" << static_cast<int>(sorted_freqs[i].first) 
           << ",\"count\":" << sorted_freqs[i].second << "}";
        if (i + 1 < limit) ss << ",";
    }
    ss << "]\n";
    ss << "}";
    return ss.str();
}

std::string AlgorithmResult::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"algorithmName\": \"" << algorithm_name << "\",\n";
    ss << "  \"algorithmSlug\": \"" << algorithm_slug << "\",\n";
    ss << "  \"originalSize\": " << original_size << ",\n";
    ss << "  \"compressedSize\": " << compressed_size << ",\n";
    ss << "  \"payloadSize\": " << payload_size << ",\n";
    ss << "  \"metadataSize\": " << metadata_size << ",\n";
    ss << "  \"compressionRatio\": " << std::fixed << std::setprecision(4) << compression_ratio << ",\n";
    ss << "  \"spaceSavedPercent\": " << std::fixed << std::setprecision(2) << space_saved_percent << ",\n";
    ss << "  \"compressionTimeMs\": " << std::fixed << std::setprecision(3) << compression_time_ms << ",\n";
    ss << "  \"decompressionTimeMs\": " << std::fixed << std::setprecision(3) << decompression_time_ms << ",\n";
    ss << "  \"compressionThroughput\": " << std::fixed << std::setprecision(2) << compression_throughput_mb_s << ",\n";
    ss << "  \"decompressionThroughput\": " << std::fixed << std::setprecision(2) << decompression_throughput_mb_s << ",\n";
    ss << "  \"integrityVerified\": " << (integrity_verified ? "true" : "false") << ",\n";
    ss << "  \"originalCrc32\": " << original_crc32 << ",\n";
    ss << "  \"restoredCrc32\": " << restored_crc32 << ",\n";
    ss << "  \"theoreticalTimeComplexity\": \"" << theoretical_time_complexity << "\",\n";
    ss << "  \"theoreticalSpaceComplexity\": \"" << theoretical_space_complexity << "\"\n";
    ss << "}";
    return ss.str();
}

} // namespace sfcw
