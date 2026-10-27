#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace sfcw {

struct HuffmanMergeEvent {
    int step = 0;
    int node_a_id = -1;
    int node_a_symbol = -1; // -1 if internal
    uint64_t node_a_freq = 0;
    int node_b_id = -1;
    int node_b_symbol = -1;
    uint64_t node_b_freq = 0;
    int parent_id = -1;
    uint64_t combined_freq = 0;
    std::vector<int> queue_node_ids; // snapshot of min-priority queue IDs
};

struct HuffmanTreeNodeInfo {
    int id = -1;
    int symbol = -1; // -1 if internal
    uint64_t freq = 0;
    bool is_leaf = false;
    int left_id = -1;
    int right_id = -1;
    std::string code;
};

struct HuffmanCodeEntry {
    uint8_t symbol = 0;
    uint64_t freq = 0;
    std::string code;
    size_t bit_length = 0;
};

struct HuffmanTrace {
    std::vector<HuffmanCodeEntry> frequencies;
    std::vector<HuffmanMergeEvent> merge_steps;
    std::vector<HuffmanTreeNodeInfo> tree_nodes;
    std::vector<HuffmanCodeEntry> codebook;
    std::string sample_bitstream; // first up to 256 bits
    uint64_t total_payload_bits = 0;

    std::string to_json() const;
};

struct LzwStepEvent {
    int step = 0;
    uint8_t byte_val = 0;
    std::string phrase_before_hex;
    std::string combined_phrase_hex;
    bool found_in_dict = false;
    int emitted_code = -1; // -1 if none
    int added_code = -1;   // -1 if none
    std::string added_phrase_hex;
    size_t dict_size = 0;
};

struct LzwTrace {
    std::vector<LzwStepEvent> steps;
    size_t total_input_bytes_traced = 0;
    size_t final_dict_size = 0;

    std::string to_json() const;
};

struct RleStepEvent {
    int step = 0;
    uint8_t byte_val = 0;
    uint8_t run_length = 0;
    size_t original_offset = 0;
};

struct RleTrace {
    std::vector<RleStepEvent> runs;
    size_t total_runs = 0;
    size_t encoded_bytes = 0;

    std::string to_json() const;
};

class AlgorithmTracer {
public:
    explicit AlgorithmTracer(size_t max_events = 500) : max_events_(max_events) {}

    bool should_record() const {
        return event_count_ < max_events_;
    }

    void increment_event() {
        event_count_++;
    }

    size_t max_events() const { return max_events_; }
    size_t event_count() const { return event_count_; }

    HuffmanTrace huffman;
    LzwTrace lzw;
    RleTrace rle;

private:
    size_t max_events_;
    size_t event_count_ = 0;
};

} // namespace sfcw
