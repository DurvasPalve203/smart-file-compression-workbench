#include "lzw.hpp"
#include <sstream>
#include <iomanip>

namespace sfcw {

static std::string bytes_to_hex_preview(const uint8_t* data, size_t len, size_t max_bytes = 8) {
    std::ostringstream ss;
    size_t count = std::min(len, max_bytes);
    for (size_t i = 0; i < count; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
        if (i + 1 < count) ss << " ";
    }
    if (len > max_bytes) ss << "... (" << len << "B)";
    return ss.str();
}

CompressedContainer LzwEngine::compress(const uint8_t* data, size_t size, AlgorithmTracer* tracer) {
    CompressedContainer container;
    container.header.algorithm = AlgorithmId::LZW;
    container.header.original_size = size;
    container.header.original_crc32 = Crc32::calculate(data, size);

    // Metadata: [1 byte code_width = 16][4 bytes max_dict = 65536][4 bytes init_dict = 256]
    container.algorithm_metadata.clear();
    container.algorithm_metadata.push_back(16); // 16-bit
    write_u32_be(container.algorithm_metadata, 65536);
    write_u32_be(container.algorithm_metadata, 256);

    container.header.metadata_size = static_cast<uint32_t>(container.algorithm_metadata.size());
    container.header.header_size = static_cast<uint32_t>(FIXED_HEADER_BYTES + container.algorithm_metadata.size());

    if (size == 0) {
        container.header.payload_size = 0;
        return container;
    }

    // Initialize phrase dictionary map: (prefix_code, char) -> new_code
    // Note: Codes 0..255 are root single bytes.
    std::unordered_map<uint32_t, uint16_t> dict;
    dict.reserve(65536);

    uint32_t next_code = 256;
    uint16_t current_phrase_code = data[0];

    std::vector<uint16_t> output_codes;
    // rough reserve estimate
    output_codes.reserve(size);

    std::vector<uint8_t> current_phrase_bytes;
    if (tracer) {
        current_phrase_bytes.push_back(data[0]);
        tracer->lzw.total_input_bytes_traced = std::min(size, static_cast<size_t>(tracer->max_events()));
    }

    int step_num = 1;

    for (size_t i = 1; i < size; ++i) {
        uint8_t next_byte = data[i];
        uint32_t key = make_dict_key(current_phrase_code, next_byte);
        auto it = dict.find(key);

        if (it != dict.end()) {
            // Phrase exists in dictionary
            if (tracer && tracer->should_record()) {
                LzwStepEvent ev;
                ev.step = step_num++;
                ev.byte_val = next_byte;
                ev.phrase_before_hex = bytes_to_hex_preview(current_phrase_bytes.data(), current_phrase_bytes.size());
                current_phrase_bytes.push_back(next_byte);
                ev.combined_phrase_hex = bytes_to_hex_preview(current_phrase_bytes.data(), current_phrase_bytes.size());
                ev.found_in_dict = true;
                ev.emitted_code = -1;
                ev.added_code = -1;
                ev.dict_size = 256 + dict.size();
                tracer->lzw.steps.push_back(ev);
                tracer->increment_event();
            }

            current_phrase_code = it->second;
        } else {
            // Phrase not in dictionary: emit current phrase code
            output_codes.push_back(current_phrase_code);

            int added_code = -1;
            std::string added_phrase_str;

            if (next_code <= LZW_MAX_CODE) {
                dict[key] = static_cast<uint16_t>(next_code);
                added_code = next_code;
                next_code++;
            }

            if (tracer && tracer->should_record()) {
                LzwStepEvent ev;
                ev.step = step_num++;
                ev.byte_val = next_byte;
                ev.phrase_before_hex = bytes_to_hex_preview(current_phrase_bytes.data(), current_phrase_bytes.size());
                
                auto combined = current_phrase_bytes;
                combined.push_back(next_byte);
                ev.combined_phrase_hex = bytes_to_hex_preview(combined.data(), combined.size());
                ev.found_in_dict = false;
                ev.emitted_code = current_phrase_code;
                ev.added_code = added_code;
                ev.added_phrase_hex = bytes_to_hex_preview(combined.data(), combined.size());
                ev.dict_size = 256 + dict.size();
                tracer->lzw.steps.push_back(ev);
                tracer->increment_event();

                current_phrase_bytes.clear();
                current_phrase_bytes.push_back(next_byte);
            }

            current_phrase_code = next_byte;
        }
    }

    // Emit final remaining phrase
    output_codes.push_back(current_phrase_code);

    if (tracer) {
        tracer->lzw.final_dict_size = 256 + dict.size();
    }

    // Serialize 16-bit codes to big-endian bytes
    container.payload.reserve(output_codes.size() * 2);
    for (uint16_t code : output_codes) {
        write_u16_be(container.payload, code);
    }
    container.header.payload_size = container.payload.size();

    return container;
}

std::vector<uint8_t> LzwEngine::decompress(const CompressedContainer& container) {
    if (container.header.algorithm != AlgorithmId::LZW) {
        throw std::runtime_error("Container algorithm is not LZW");
    }

    if (container.header.original_size == 0) {
        return {};
    }

    if (container.payload.size() % 2 != 0) {
        throw std::runtime_error("Corrupted LZW payload: byte length is not a multiple of 16-bit codes");
    }

    size_t num_codes = container.payload.size() / 2;
    if (num_codes == 0) {
        throw std::runtime_error("Corrupted LZW container: non-empty original size but 0 codes");
    }

    // Read all 16-bit codes
    std::vector<uint16_t> codes;
    codes.reserve(num_codes);
    size_t offset = 0;
    for (size_t i = 0; i < num_codes; ++i) {
        codes.push_back(read_u16_be(container.payload.data(), offset, container.payload.size()));
    }

    // Initialize dictionary table with single byte entries
    std::vector<std::vector<uint8_t>> dict;
    dict.reserve(65536);
    for (int i = 0; i < 256; ++i) {
        dict.push_back({static_cast<uint8_t>(i)});
    }

    std::vector<uint8_t> decoded;
    decoded.reserve(container.header.original_size);

    uint16_t prev_code = codes[0];
    if (prev_code >= dict.size()) {
        throw std::runtime_error("Corrupted LZW data: first code " + std::to_string(prev_code) + " out of bounds");
    }

    decoded.insert(decoded.end(), dict[prev_code].begin(), dict[prev_code].end());

    for (size_t i = 1; i < num_codes; ++i) {
        uint16_t curr_code = codes[i];
        std::vector<uint8_t> entry;

        if (curr_code < dict.size()) {
            entry = dict[curr_code];
        } else if (curr_code == dict.size()) {
            // Standard LZW special case: cWcWc pattern
            entry = dict[prev_code];
            entry.push_back(dict[prev_code][0]);
        } else {
            throw std::runtime_error("Corrupted LZW data: code " + std::to_string(curr_code) + " exceeds dictionary capacity");
        }

        decoded.insert(decoded.end(), entry.begin(), entry.end());

        // Add new entry to dictionary if capacity remains
        if (dict.size() <= LZW_MAX_CODE) {
            std::vector<uint8_t> new_entry = dict[prev_code];
            new_entry.push_back(entry[0]);
            dict.push_back(std::move(new_entry));
        }

        prev_code = curr_code;
    }

    // Verify size & CRC
    if (decoded.size() != container.header.original_size) {
        throw std::runtime_error("LZW decompression size mismatch: expected " + 
                                 std::to_string(container.header.original_size) + 
                                 " bytes, got " + std::to_string(decoded.size()));
    }

    uint32_t computed_crc = Crc32::calculate(decoded);
    if (computed_crc != container.header.original_crc32) {
        throw std::runtime_error("LZW decompression CRC32 checksum mismatch: data corruption detected");
    }

    return decoded;
}

} // namespace sfcw
