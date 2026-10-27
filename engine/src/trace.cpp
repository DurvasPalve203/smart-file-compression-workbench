#include "trace.hpp"
#include <iomanip>

namespace sfcw {

std::string HuffmanTrace::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"algorithm\": \"huffman\",\n";
    ss << "  \"totalPayloadBits\": " << total_payload_bits << ",\n";
    ss << "  \"sampleBitstream\": \"" << sample_bitstream << "\",\n";

    // Frequencies
    ss << "  \"frequencies\": [";
    for (size_t i = 0; i < frequencies.size(); ++i) {
        ss << "{\"symbol\":" << static_cast<int>(frequencies[i].symbol)
           << ",\"freq\":" << frequencies[i].freq << "}";
        if (i + 1 < frequencies.size()) ss << ",";
    }
    ss << "],\n";

    // Merge steps
    ss << "  \"mergeSteps\": [";
    for (size_t i = 0; i < merge_steps.size(); ++i) {
        const auto& m = merge_steps[i];
        ss << "{\"step\":" << m.step
           << ",\"nodeA\":{\"id\":" << m.node_a_id << ",\"symbol\":" << m.node_a_symbol << ",\"freq\":" << m.node_a_freq << "}"
           << ",\"nodeB\":{\"id\":" << m.node_b_id << ",\"symbol\":" << m.node_b_symbol << ",\"freq\":" << m.node_b_freq << "}"
           << ",\"parent\":{\"id\":" << m.parent_id << ",\"freq\":" << m.combined_freq << "}"
           << ",\"queue\":[";
        for (size_t q = 0; q < m.queue_node_ids.size(); ++q) {
            ss << m.queue_node_ids[q];
            if (q + 1 < m.queue_node_ids.size()) ss << ",";
        }
        ss << "]}";
        if (i + 1 < merge_steps.size()) ss << ",";
    }
    ss << "],\n";

    // Tree nodes
    ss << "  \"treeNodes\": [";
    for (size_t i = 0; i < tree_nodes.size(); ++i) {
        const auto& n = tree_nodes[i];
        ss << "{\"id\":" << n.id
           << ",\"symbol\":" << n.symbol
           << ",\"freq\":" << n.freq
           << ",\"isLeaf\":" << (n.is_leaf ? "true" : "false")
           << ",\"leftId\":" << n.left_id
           << ",\"rightId\":" << n.right_id
           << ",\"code\":\"" << n.code << "\"}";
        if (i + 1 < tree_nodes.size()) ss << ",";
    }
    ss << "],\n";

    // Codebook
    ss << "  \"codebook\": [";
    for (size_t i = 0; i < codebook.size(); ++i) {
        const auto& c = codebook[i];
        ss << "{\"symbol\":" << static_cast<int>(c.symbol)
           << ",\"freq\":" << c.freq
           << ",\"code\":\"" << c.code << "\""
           << ",\"bits\":" << c.bit_length << "}";
        if (i + 1 < codebook.size()) ss << ",";
    }
    ss << "]\n";
    ss << "}";
    return ss.str();
}

std::string LzwTrace::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"algorithm\": \"lzw\",\n";
    ss << "  \"totalInputBytesTraced\": " << total_input_bytes_traced << ",\n";
    ss << "  \"finalDictSize\": " << final_dict_size << ",\n";
    ss << "  \"steps\": [";
    for (size_t i = 0; i < steps.size(); ++i) {
        const auto& s = steps[i];
        ss << "{\"step\":" << s.step
           << ",\"byteVal\":" << static_cast<int>(s.byte_val)
           << ",\"phraseBefore\":\"" << s.phrase_before_hex << "\""
           << ",\"combinedPhrase\":\"" << s.combined_phrase_hex << "\""
           << ",\"foundInDict\":" << (s.found_in_dict ? "true" : "false")
           << ",\"emittedCode\":" << s.emitted_code
           << ",\"addedCode\":" << s.added_code
           << ",\"addedPhrase\":\"" << s.added_phrase_hex << "\""
           << ",\"dictSize\":" << s.dict_size << "}";
        if (i + 1 < steps.size()) ss << ",";
    }
    ss << "]\n";
    ss << "}";
    return ss.str();
}

std::string RleTrace::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"algorithm\": \"rle\",\n";
    ss << "  \"totalRuns\": " << total_runs << ",\n";
    ss << "  \"encodedBytes\": " << encoded_bytes << ",\n";
    ss << "  \"runs\": [";
    for (size_t i = 0; i < runs.size(); ++i) {
        const auto& r = runs[i];
        ss << "{\"step\":" << r.step
           << ",\"byteVal\":" << static_cast<int>(r.byte_val)
           << ",\"runLength\":" << static_cast<int>(r.run_length)
           << ",\"offset\":" << r.original_offset << "}";
        if (i + 1 < runs.size()) ss << ",";
    }
    ss << "]\n";
    ss << "}";
    return ss.str();
}

} // namespace sfcw
