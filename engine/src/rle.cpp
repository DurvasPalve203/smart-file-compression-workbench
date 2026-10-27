#include "rle.hpp"

namespace sfcw {

CompressedContainer RleEngine::compress(const uint8_t* data, size_t size, AlgorithmTracer* tracer) {
    CompressedContainer container;
    container.header.algorithm = AlgorithmId::RLE;
    container.header.original_size = size;
    container.header.original_crc32 = Crc32::calculate(data, size);

    // Metadata: [1 byte format=1][1 byte max_run=255]
    container.algorithm_metadata = {1, 255};
    container.header.metadata_size = 2;
    container.header.header_size = FIXED_HEADER_BYTES + 2;

    if (size == 0) {
        container.header.payload_size = 0;
        return container;
    }

    size_t i = 0;
    int step_num = 1;
    while (i < size) {
        uint8_t byte_val = data[i];
        size_t run_len = 1;
        while (i + run_len < size && data[i + run_len] == byte_val && run_len < 255) {
            run_len++;
        }

        container.payload.push_back(static_cast<uint8_t>(run_len));
        container.payload.push_back(byte_val);

        if (tracer && tracer->should_record()) {
            RleStepEvent ev;
            ev.step = step_num++;
            ev.byte_val = byte_val;
            ev.run_length = static_cast<uint8_t>(run_len);
            ev.original_offset = i;
            tracer->rle.runs.push_back(ev);
            tracer->increment_event();
        }

        i += run_len;
    }

    if (tracer) {
        tracer->rle.total_runs = container.payload.size() / 2;
        tracer->rle.encoded_bytes = container.payload.size();
    }

    container.header.payload_size = container.payload.size();
    return container;
}

std::vector<uint8_t> RleEngine::decompress(const CompressedContainer& container) {
    if (container.header.algorithm != AlgorithmId::RLE) {
        throw std::runtime_error("Container algorithm is not RLE");
    }

    if (container.header.original_size == 0) {
        return {};
    }

    if (container.payload.size() % 2 != 0) {
        throw std::runtime_error("Corrupted RLE payload: payload byte count must be even (count, val pairs)");
    }

    std::vector<uint8_t> decoded;
    decoded.reserve(container.header.original_size);

    for (size_t i = 0; i < container.payload.size(); i += 2) {
        uint8_t count = container.payload[i];
        uint8_t val = container.payload[i + 1];

        if (count == 0) {
            throw std::runtime_error("Corrupted RLE data: run count cannot be zero");
        }

        if (decoded.size() + count > container.header.original_size) {
            throw std::runtime_error("Corrupted RLE data: decompressed size exceeds declared original size");
        }

        decoded.insert(decoded.end(), count, val);
    }

    if (decoded.size() != container.header.original_size) {
        throw std::runtime_error("RLE decompression size mismatch: expected " + 
                                 std::to_string(container.header.original_size) + 
                                 " bytes, got " + std::to_string(decoded.size()));
    }

    uint32_t computed_crc = Crc32::calculate(decoded);
    if (computed_crc != container.header.original_crc32) {
        throw std::runtime_error("RLE decompression CRC32 checksum mismatch: data corruption detected");
    }

    return decoded;
}

} // namespace sfcw
