#pragma once

#include "container.hpp"
#include "trace.hpp"
#include <vector>

namespace sfcw {

class RleEngine {
public:
    static CompressedContainer compress(const uint8_t* data, size_t size, AlgorithmTracer* tracer = nullptr);
    static std::vector<uint8_t> decompress(const CompressedContainer& container);
};

} // namespace sfcw
