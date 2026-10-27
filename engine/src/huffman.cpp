#include "huffman.hpp"
#include <algorithm>
#include <stdexcept>

namespace sfcw {

void HuffmanEngine::generate_codes(const std::shared_ptr<HuffmanNode>& node, 
                                   const std::string& current_code, 
                                   std::string codebook[256],
                                   std::vector<HuffmanTreeNodeInfo>* tree_nodes) {
    if (!node) return;

    if (tree_nodes) {
        HuffmanTreeNodeInfo info;
        info.id = node->id;
        info.symbol = node->symbol;
        info.freq = node->freq;
        info.is_leaf = node->is_leaf();
        info.left_id = node->left ? node->left->id : -1;
        info.right_id = node->right ? node->right->id : -1;
        info.code = current_code;
        tree_nodes->push_back(info);
    }

    if (node->is_leaf()) {
        codebook[node->symbol] = current_code.empty() ? "0" : current_code;
        return;
    }

    if (node->left) {
        generate_codes(node->left, current_code + "0", codebook, tree_nodes);
    }
    if (node->right) {
        generate_codes(node->right, current_code + "1", codebook, tree_nodes);
    }
}

CompressedContainer HuffmanEngine::compress(const uint8_t* data, size_t size, AlgorithmTracer* tracer) {
    CompressedContainer container;
    container.header.algorithm = AlgorithmId::HUFFMAN;
    container.header.original_size = size;
    container.header.original_crc32 = Crc32::calculate(data, size);

    if (size == 0) {
        // Empty file handling
        container.header.metadata_size = 3; // 1 byte valid_bits + 2 bytes symbol_count(0)
        container.algorithm_metadata.resize(3, 0);
        container.header.payload_size = 0;
        container.header.header_size = FIXED_HEADER_BYTES + 3;
        return container;
    }

    // 1. Calculate frequency table
    uint64_t frequencies[256] = {0};
    for (size_t i = 0; i < size; ++i) {
        frequencies[data[i]]++;
    }

    uint16_t distinct_symbols = 0;
    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] > 0) distinct_symbols++;
    }

    if (tracer) {
        for (int i = 0; i < 256; ++i) {
            if (frequencies[i] > 0) {
                HuffmanCodeEntry entry;
                entry.symbol = static_cast<uint8_t>(i);
                entry.freq = frequencies[i];
                tracer->huffman.frequencies.push_back(entry);
            }
        }
    }

    // 2. Build Min-Priority Queue with deterministic tie-breaking
    std::priority_queue<std::shared_ptr<HuffmanNode>, 
                        std::vector<std::shared_ptr<HuffmanNode>>, 
                        HuffmanNodeComparator> pq;

    int next_node_id = 0;
    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] > 0) {
            auto leaf = std::make_shared<HuffmanNode>();
            leaf->id = next_node_id++;
            leaf->symbol = i;
            leaf->freq = frequencies[i];
            leaf->min_symbol = i;
            pq.push(leaf);
        }
    }

    std::shared_ptr<HuffmanNode> root = nullptr;

    if (distinct_symbols == 1) {
        // Special case: single unique symbol
        auto single_leaf = pq.top();
        pq.pop();
        root = std::make_shared<HuffmanNode>();
        root->id = next_node_id++;
        root->symbol = -1;
        root->freq = single_leaf->freq;
        root->min_symbol = single_leaf->min_symbol;
        root->left = single_leaf;
        root->right = nullptr;
    } else {
        int step = 1;
        while (pq.size() > 1) {
            auto node_a = pq.top(); pq.pop();
            auto node_b = pq.top(); pq.pop();

            auto parent = std::make_shared<HuffmanNode>();
            parent->id = next_node_id++;
            parent->symbol = -1;
            parent->freq = node_a->freq + node_b->freq;
            parent->min_symbol = std::min(node_a->min_symbol, node_b->min_symbol);
            parent->left = node_a;
            parent->right = node_b;

            if (tracer && tracer->should_record()) {
                HuffmanMergeEvent m;
                m.step = step++;
                m.node_a_id = node_a->id;
                m.node_a_symbol = node_a->symbol;
                m.node_a_freq = node_a->freq;
                m.node_b_id = node_b->id;
                m.node_b_symbol = node_b->symbol;
                m.node_b_freq = node_b->freq;
                m.parent_id = parent->id;
                m.combined_freq = parent->freq;

                // Snapshot current queue ids
                auto temp_pq = pq;
                while (!temp_pq.empty()) {
                    m.queue_node_ids.push_back(temp_pq.top()->id);
                    temp_pq.pop();
                }
                tracer->huffman.merge_steps.push_back(m);
                tracer->increment_event();
            }

            pq.push(parent);
        }
        root = pq.top();
    }

    // 3. Generate prefix-free codes
    std::string codebook[256];
    std::vector<HuffmanTreeNodeInfo>* tree_nodes_ptr = tracer ? &tracer->huffman.tree_nodes : nullptr;
    generate_codes(root, "", codebook, tree_nodes_ptr);

    if (tracer) {
        for (int i = 0; i < 256; ++i) {
            if (frequencies[i] > 0) {
                HuffmanCodeEntry entry;
                entry.symbol = static_cast<uint8_t>(i);
                entry.freq = frequencies[i];
                entry.code = codebook[i];
                entry.bit_length = codebook[i].length();
                tracer->huffman.codebook.push_back(entry);
            }
        }
    }

    // 4. Encode payload with BitWriter
    BitWriter bit_writer;
    for (size_t i = 0; i < size; ++i) {
        const std::string& code = codebook[data[i]];
        bit_writer.write_bits(code);
        if (tracer && tracer->huffman.sample_bitstream.length() < 256) {
            tracer->huffman.sample_bitstream += code;
        }
    }

    uint8_t valid_bits_in_last_byte = 0;
    container.payload = bit_writer.finish(valid_bits_in_last_byte);
    container.header.payload_size = container.payload.size();

    if (tracer) {
        tracer->huffman.total_payload_bits = bit_writer.total_bits();
    }

    // 5. Serialize algorithm metadata:
    // [1 byte valid_bits][2 bytes distinct_symbols_count][for each: 1 byte symbol + 8 bytes freq]
    container.algorithm_metadata.clear();
    container.algorithm_metadata.push_back(valid_bits_in_last_byte);
    write_u16_be(container.algorithm_metadata, distinct_symbols);

    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] > 0) {
            container.algorithm_metadata.push_back(static_cast<uint8_t>(i));
            write_u64_be(container.algorithm_metadata, frequencies[i]);
        }
    }

    container.header.metadata_size = static_cast<uint32_t>(container.algorithm_metadata.size());
    container.header.header_size = static_cast<uint32_t>(FIXED_HEADER_BYTES + container.algorithm_metadata.size());

    return container;
}

std::vector<uint8_t> HuffmanEngine::decompress(const CompressedContainer& container) {
    if (container.header.algorithm != AlgorithmId::HUFFMAN) {
        throw std::runtime_error("Container algorithm is not Huffman");
    }

    if (container.header.original_size == 0) {
        return {};
    }

    // 1. Read metadata
    size_t offset = 0;
    const auto& meta = container.algorithm_metadata;
    if (meta.size() < 3) {
        throw std::runtime_error("Corrupted Huffman metadata: truncated header");
    }

    uint8_t valid_bits_in_last_byte = meta[offset++];
    uint16_t distinct_symbols = read_u16_be(meta.data(), offset, meta.size());

    if (meta.size() < 3 + distinct_symbols * 9) {
        throw std::runtime_error("Corrupted Huffman metadata: insufficient frequency table entries");
    }

    uint64_t frequencies[256] = {0};
    for (uint16_t i = 0; i < distinct_symbols; ++i) {
        uint8_t sym = meta[offset++];
        uint64_t freq = read_u64_be(meta.data(), offset, meta.size());
        frequencies[sym] = freq;
    }

    // 2. Rebuild the exact same Huffman tree
    std::priority_queue<std::shared_ptr<HuffmanNode>, 
                        std::vector<std::shared_ptr<HuffmanNode>>, 
                        HuffmanNodeComparator> pq;

    int next_node_id = 0;
    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] > 0) {
            auto leaf = std::make_shared<HuffmanNode>();
            leaf->id = next_node_id++;
            leaf->symbol = i;
            leaf->freq = frequencies[i];
            leaf->min_symbol = i;
            pq.push(leaf);
        }
    }

    if (pq.empty()) {
        throw std::runtime_error("Corrupted Huffman metadata: no active symbols");
    }

    std::shared_ptr<HuffmanNode> root = nullptr;
    if (distinct_symbols == 1) {
        auto single_leaf = pq.top();
        pq.pop();
        root = std::make_shared<HuffmanNode>();
        root->id = next_node_id++;
        root->symbol = -1;
        root->freq = single_leaf->freq;
        root->min_symbol = single_leaf->min_symbol;
        root->left = single_leaf;
        root->right = nullptr;
    } else {
        while (pq.size() > 1) {
            auto node_a = pq.top(); pq.pop();
            auto node_b = pq.top(); pq.pop();

            auto parent = std::make_shared<HuffmanNode>();
            parent->id = next_node_id++;
            parent->symbol = -1;
            parent->freq = node_a->freq + node_b->freq;
            parent->min_symbol = std::min(node_a->min_symbol, node_b->min_symbol);
            parent->left = node_a;
            parent->right = node_b;
            pq.push(parent);
        }
        root = pq.top();
    }

    // 3. Decode bitstream
    uint64_t total_payload_bits = 0;
    if (!container.payload.empty()) {
        total_payload_bits = (container.payload.size() - 1) * 8 + valid_bits_in_last_byte;
    }

    BitReader reader(container.payload.data(), container.payload.size(), total_payload_bits);
    std::vector<uint8_t> decoded;
    decoded.reserve(container.header.original_size);

    auto curr = root;
    int bit = 0;
    while (reader.read_bit(bit)) {
        if (bit == 0) {
            curr = curr->left;
        } else {
            curr = curr->right;
        }

        if (!curr) {
            throw std::runtime_error("Corrupted Huffman bitstream: invalid tree traversal path");
        }

        if (curr->is_leaf()) {
            decoded.push_back(static_cast<uint8_t>(curr->symbol));
            curr = root;
            if (decoded.size() == container.header.original_size) {
                break; // Fully decoded
            }
        }
    }

    // 4. Verify decoded size and CRC32
    if (decoded.size() != container.header.original_size) {
        throw std::runtime_error("Huffman decompression size mismatch: expected " + 
                                 std::to_string(container.header.original_size) + 
                                 " bytes, got " + std::to_string(decoded.size()));
    }

    uint32_t computed_crc = Crc32::calculate(decoded);
    if (computed_crc != container.header.original_crc32) {
        throw std::runtime_error("Huffman decompression CRC32 checksum mismatch: data corruption detected");
    }

    return decoded;
}

} // namespace sfcw
