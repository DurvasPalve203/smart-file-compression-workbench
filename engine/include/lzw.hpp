#pragma once

#include "container.hpp"
#include "trace.hpp"
#include <vector>
#include <unordered_map>
#include <string>

namespace sfcw {

constexpr uint16_t LZW_MAX_CODE = 65535;
constexpr size_t LZW_INITIAL_DICT_SIZE = 256;

class LzwEngine {
public:
    static CompressedContainer compress(const uint8_t* data, size_t size, AlgorithmTracer* tracer = nullptr);
    static std::vector<uint8_t> decompress(const CompressedContainer& container);

private:
    static inline uint32_t make_dict_key(uint16_t prefix_code, uint8_t next_byte) {
        return (static_cast<uint32_t>(prefix_code) << 8) | static_cast<uint32_t>(next_byte);
    }
};

} // namespace sfcw
