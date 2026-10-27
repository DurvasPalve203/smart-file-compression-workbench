#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <iostream>

namespace sfcw {

// Magic 4 bytes: "SFCW" (Smart File Compression Workbench)
constexpr uint32_t SFCW_MAGIC = 0x53464357; 
constexpr uint16_t SFCW_VERSION = 1;

enum class AlgorithmId : uint16_t {
    HUFFMAN = 1,
    LZW     = 2,
    RLE     = 3
};

inline const char* algorithm_name(AlgorithmId id) {
    switch (id) {
        case AlgorithmId::HUFFMAN: return "Huffman Coding";
        case AlgorithmId::LZW:     return "LZW Compression";
        case AlgorithmId::RLE:     return "Run-Length Encoding";
        default:                   return "Unknown Algorithm";
    }
}

inline const char* algorithm_slug(AlgorithmId id) {
    switch (id) {
        case AlgorithmId::HUFFMAN: return "huffman";
        case AlgorithmId::LZW:     return "lzw";
        case AlgorithmId::RLE:     return "rle";
        default:                   return "unknown";
    }
}

inline AlgorithmId parse_algorithm_slug(const std::string& slug) {
    if (slug == "huffman") return AlgorithmId::HUFFMAN;
    if (slug == "lzw")     return AlgorithmId::LZW;
    if (slug == "rle")     return AlgorithmId::RLE;
    throw std::runtime_error("Unknown algorithm slug: " + slug);
}

// CRC32 calculation table and utility
class Crc32 {
public:
    static uint32_t calculate(const uint8_t* data, size_t length) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < length; ++i) {
            uint8_t byte = data[i];
            uint8_t lookup_index = (crc ^ byte) & 0xFF;
            crc = (crc >> 8) ^ table[lookup_index];
        }
        return crc ^ 0xFFFFFFFF;
    }

    static uint32_t calculate(const std::vector<uint8_t>& data) {
        return calculate(data.data(), data.size());
    }

private:
    static const uint32_t table[256];
};

// Endianness serialization utilities (Big-Endian network order)
inline void write_u16_be(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

inline void write_u32_be(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

inline void write_u64_be(std::vector<uint8_t>& buf, uint64_t val) {
    for (int i = 7; i >= 0; --i) {
        buf.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
    }
}

inline uint16_t read_u16_be(const uint8_t* buf, size_t& offset, size_t total_size) {
    if (offset + 2 > total_size) throw std::runtime_error("Unexpected EOF reading uint16");
    uint16_t val = (static_cast<uint16_t>(buf[offset]) << 8) |
                   (static_cast<uint16_t>(buf[offset + 1]));
    offset += 2;
    return val;
}

inline uint32_t read_u32_be(const uint8_t* buf, size_t& offset, size_t total_size) {
    if (offset + 4 > total_size) throw std::runtime_error("Unexpected EOF reading uint32");
    uint32_t val = (static_cast<uint32_t>(buf[offset]) << 24) |
                   (static_cast<uint32_t>(buf[offset + 1]) << 16) |
                   (static_cast<uint32_t>(buf[offset + 2]) << 8) |
                   (static_cast<uint32_t>(buf[offset + 3]));
    offset += 4;
    return val;
}

inline uint64_t read_u64_be(const uint8_t* buf, size_t& offset, size_t total_size) {
    if (offset + 8 > total_size) throw std::runtime_error("Unexpected EOF reading uint64");
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val = (val << 8) | static_cast<uint64_t>(buf[offset + i]);
    }
    offset += 8;
    return val;
}

// Container Header struct
struct ContainerHeader {
    uint32_t magic = SFCW_MAGIC;
    uint16_t version = SFCW_VERSION;
    AlgorithmId algorithm = AlgorithmId::HUFFMAN;
    uint32_t header_size = 0;
    uint64_t original_size = 0;
    uint64_t payload_size = 0;
    uint32_t original_crc32 = 0;
    uint32_t metadata_size = 0;
    uint32_t reserved = 0;
};

constexpr size_t FIXED_HEADER_BYTES = 40; // 4 + 2 + 2 + 4 + 8 + 8 + 4 + 4 + 4 = 40 bytes

struct CompressedContainer {
    ContainerHeader header;
    std::vector<uint8_t> algorithm_metadata;
    std::vector<uint8_t> payload;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> out;
        out.reserve(FIXED_HEADER_BYTES + algorithm_metadata.size() + payload.size());

        write_u32_be(out, header.magic);
        write_u16_be(out, header.version);
        write_u16_be(out, static_cast<uint16_t>(header.algorithm));
        write_u32_be(out, static_cast<uint32_t>(FIXED_HEADER_BYTES + algorithm_metadata.size()));
        write_u64_be(out, header.original_size);
        write_u64_be(out, header.payload_size);
        write_u32_be(out, header.original_crc32);
        write_u32_be(out, static_cast<uint32_t>(algorithm_metadata.size()));
        write_u32_be(out, 0); // reserved

        out.insert(out.end(), algorithm_metadata.begin(), algorithm_metadata.end());
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

    static CompressedContainer deserialize(const uint8_t* data, size_t size) {
        if (size < FIXED_HEADER_BYTES) {
            throw std::runtime_error("File size is smaller than container header");
        }

        size_t offset = 0;
        CompressedContainer container;
        container.header.magic = read_u32_be(data, offset, size);
        if (container.header.magic != SFCW_MAGIC) {
            throw std::runtime_error("Invalid SFCW magic signature. Not a Smart File Compression Workbench file.");
        }

        container.header.version = read_u16_be(data, offset, size);
        if (container.header.version != SFCW_VERSION) {
            throw std::runtime_error("Unsupported container version: " + std::to_string(container.header.version));
        }

        uint16_t algo_val = read_u16_be(data, offset, size);
        if (algo_val < 1 || algo_val > 3) {
            throw std::runtime_error("Unknown algorithm ID in container header: " + std::to_string(algo_val));
        }
        container.header.algorithm = static_cast<AlgorithmId>(algo_val);

        container.header.header_size = read_u32_be(data, offset, size);
        container.header.original_size = read_u64_be(data, offset, size);
        container.header.payload_size = read_u64_be(data, offset, size);
        container.header.original_crc32 = read_u32_be(data, offset, size);
        container.header.metadata_size = read_u32_be(data, offset, size);
        container.header.reserved = read_u32_be(data, offset, size);

        if (container.header.header_size != FIXED_HEADER_BYTES + container.header.metadata_size) {
            throw std::runtime_error("Inconsistent header and metadata sizes in container");
        }

        if (size < container.header.header_size + container.header.payload_size) {
            throw std::runtime_error("Container is truncated. File size is smaller than declared payload");
        }

        if (container.header.metadata_size > 0) {
            container.algorithm_metadata.assign(data + offset, data + offset + container.header.metadata_size);
            offset += container.header.metadata_size;
        }

        if (container.header.payload_size > 0) {
            container.payload.assign(data + offset, data + offset + container.header.payload_size);
            offset += container.header.payload_size;
        }

        return container;
    }
};

// File I/O helpers
inline std::vector<uint8_t> read_file_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for reading: " + path);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (size > 0) {
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            throw std::runtime_error("Failed to read file contents: " + path);
        }
    }
    return buffer;
}

inline void write_file_bytes(const std::string& path, const uint8_t* data, size_t size) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }
    if (size > 0) {
        file.write(reinterpret_cast<const char*>(data), size);
        if (!file.good()) {
            throw std::runtime_error("Failed to write data to file: " + path);
        }
    }
}

inline void write_file_bytes(const std::string& path, const std::vector<uint8_t>& data) {
    write_file_bytes(path, data.data(), data.size());
}

} // namespace sfcw
