#pragma once

#include "container.hpp"
#include "trace.hpp"
#include <vector>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>

namespace sfcw {

class BitWriter {
public:
    void write_bit(int bit) {
        if (bit) {
            current_byte_ |= (1 << (7 - bit_count_));
        }
        bit_count_++;
        total_bits_++;
        if (bit_count_ == 8) {
            bytes_.push_back(current_byte_);
            current_byte_ = 0;
            bit_count_ = 0;
        }
    }

    void write_bits(const std::string& bit_str) {
        for (char c : bit_str) {
            write_bit(c == '1' ? 1 : 0);
        }
    }

    std::vector<uint8_t> finish(uint8_t& valid_bits_in_last_byte) {
        if (bit_count_ > 0) {
            bytes_.push_back(current_byte_);
            valid_bits_in_last_byte = static_cast<uint8_t>(bit_count_);
        } else {
            valid_bits_in_last_byte = bytes_.empty() ? 0 : 8;
        }
        return bytes_;
    }

    uint64_t total_bits() const { return total_bits_; }

private:
    std::vector<uint8_t> bytes_;
    uint8_t current_byte_ = 0;
    int bit_count_ = 0;
    uint64_t total_bits_ = 0;
};

class BitReader {
public:
    BitReader(const uint8_t* data, size_t size, uint64_t total_bits)
        : data_(data), size_(size), total_bits_(total_bits) {}

    bool read_bit(int& bit) {
        if (bits_read_ >= total_bits_) return false;
        size_t byte_idx = bits_read_ / 8;
        int bit_offset = bits_read_ % 8;
        if (byte_idx >= size_) return false;

        bit = (data_[byte_idx] >> (7 - bit_offset)) & 1;
        bits_read_++;
        return true;
    }

    uint64_t bits_read() const { return bits_read_; }
    uint64_t total_bits() const { return total_bits_; }

private:
    const uint8_t* data_;
    size_t size_;
    uint64_t total_bits_;
    uint64_t bits_read_ = 0;
};

struct HuffmanNode {
    int id = -1;
    int symbol = -1; // -1 for internal node, 0..255 for leaf
    uint64_t freq = 0;
    int min_symbol = 256; // used for deterministic tie-breaking
    std::shared_ptr<HuffmanNode> left;
    std::shared_ptr<HuffmanNode> right;

    bool is_leaf() const { return left == nullptr && right == nullptr; }
};

struct HuffmanNodeComparator {
    bool operator()(const std::shared_ptr<HuffmanNode>& a, const std::shared_ptr<HuffmanNode>& b) const {
        if (a->freq != b->freq) {
            return a->freq > b->freq; // Min-heap: lowest frequency first
        }
        if (a->min_symbol != b->min_symbol) {
            return a->min_symbol > b->min_symbol;
        }
        return a->id > b->id;
    }
};

class HuffmanEngine {
public:
    static CompressedContainer compress(const uint8_t* data, size_t size, AlgorithmTracer* tracer = nullptr);
    static std::vector<uint8_t> decompress(const CompressedContainer& container);

private:
    static void generate_codes(const std::shared_ptr<HuffmanNode>& node, 
                               const std::string& current_code, 
                               std::string codebook[256],
                               std::vector<HuffmanTreeNodeInfo>* tree_nodes = nullptr);
};

} // namespace sfcw
