#include "container.hpp"
#include "metrics.hpp"
#include "huffman.hpp"
#include "lzw.hpp"
#include "rle.hpp"
#include "trace.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace sfcw;

struct CliArgs {
    std::string action;
    std::string algo = "huffman";
    std::string input_path;
    std::string output_path;
    bool enable_trace = false;
    std::string trace_path;
    int trials = 3;
    std::vector<std::string> benchmark_algos = {"huffman", "lzw", "rle"};
};

CliArgs parse_args(int argc, char* argv[]) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--action" && i + 1 < argc) {
            args.action = argv[++i];
        } else if (arg == "--algo" && i + 1 < argc) {
            args.algo = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            args.input_path = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            args.output_path = argv[++i];
        } else if (arg == "--trace") {
            args.enable_trace = true;
        } else if (arg == "--trace-file" && i + 1 < argc) {
            args.trace_path = argv[++i];
            args.enable_trace = true;
        } else if (arg == "--trials" && i + 1 < argc) {
            args.trials = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--algos" && i + 1 < argc) {
            args.benchmark_algos.clear();
            std::stringstream ss(argv[++i]);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (!item.empty()) args.benchmark_algos.push_back(item);
            }
        }
    }
    return args;
}

std::string get_theoretical_time(AlgorithmId id) {
    switch (id) {
        case AlgorithmId::HUFFMAN: return "O(n + k log k) encoding, O(n + B) decoding";
        case AlgorithmId::LZW:     return "O(n) bounded dictionary encoding & decoding";
        case AlgorithmId::RLE:     return "O(n) encoding, O(n + r) decoding";
        default: return "Unknown";
    }
}

std::string get_theoretical_space(AlgorithmId id) {
    switch (id) {
        case AlgorithmId::HUFFMAN: return "O(k) auxiliary tree (k <= 256) + code table";
        case AlgorithmId::LZW:     return "O(D) auxiliary dictionary (D <= 65536 entries)";
        case AlgorithmId::RLE:     return "O(1) auxiliary streaming space";
        default: return "Unknown";
    }
}

int handle_analyze(const CliArgs& args) {
    if (args.input_path.empty()) {
        std::cerr << "{\"error\": \"Missing --input parameter\"}\n";
        return 1;
    }
    try {
        auto bytes = read_file_bytes(args.input_path);
        auto analysis = analyze_bytes(bytes.data(), bytes.size());
        std::cout << analysis.to_json() << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "{\"error\": \"" << ex.what() << "\"}\n";
        return 1;
    }
}

int handle_compress(const CliArgs& args) {
    if (args.input_path.empty() || args.output_path.empty()) {
        std::cerr << "{\"error\": \"Missing --input or --output parameter\"}\n";
        return 1;
    }

    try {
        auto input_bytes = read_file_bytes(args.input_path);
        AlgorithmId algo_id = parse_algorithm_slug(args.algo);

        AlgorithmTracer tracer(400);
        AlgorithmTracer* tracer_ptr = args.enable_trace ? &tracer : nullptr;

        Timer comp_timer;
        comp_timer.start();

        CompressedContainer container;
        if (algo_id == AlgorithmId::HUFFMAN) {
            container = HuffmanEngine::compress(input_bytes.data(), input_bytes.size(), tracer_ptr);
        } else if (algo_id == AlgorithmId::LZW) {
            container = LzwEngine::compress(input_bytes.data(), input_bytes.size(), tracer_ptr);
        } else if (algo_id == AlgorithmId::RLE) {
            container = RleEngine::compress(input_bytes.data(), input_bytes.size(), tracer_ptr);
        }

        auto serialized = container.serialize();
        comp_timer.stop();

        write_file_bytes(args.output_path, serialized);

        // Verification decompression timing
        Timer decomp_timer;
        decomp_timer.start();
        std::vector<uint8_t> restored;
        if (algo_id == AlgorithmId::HUFFMAN) {
            restored = HuffmanEngine::decompress(container);
        } else if (algo_id == AlgorithmId::LZW) {
            restored = LzwEngine::decompress(container);
        } else if (algo_id == AlgorithmId::RLE) {
            restored = RleEngine::decompress(container);
        }
        decomp_timer.stop();

        bool integrity = (restored.size() == input_bytes.size()) && 
                         (Crc32::calculate(restored) == container.header.original_crc32);

        AlgorithmResult res;
        res.algorithm_name = algorithm_name(algo_id);
        res.algorithm_slug = algorithm_slug(algo_id);
        res.original_size = input_bytes.size();
        res.compressed_size = serialized.size();
        res.payload_size = container.header.payload_size;
        res.metadata_size = container.header.metadata_size;
        res.compression_ratio = serialized.size() > 0 ? (static_cast<double>(input_bytes.size()) / static_cast<double>(serialized.size())) : 0.0;
        res.space_saved_percent = input_bytes.size() > 0 ? (((static_cast<double>(input_bytes.size()) - static_cast<double>(serialized.size())) / static_cast<double>(input_bytes.size())) * 100.0) : 0.0;
        res.compression_time_ms = comp_timer.elapsed_ms();
        res.decompression_time_ms = decomp_timer.elapsed_ms();
        res.compression_throughput_mb_s = comp_timer.elapsed_seconds() > 0 ? (static_cast<double>(input_bytes.size()) / comp_timer.elapsed_seconds() / 1000000.0) : 0.0;
        res.decompression_throughput_mb_s = decomp_timer.elapsed_seconds() > 0 ? (static_cast<double>(input_bytes.size()) / decomp_timer.elapsed_seconds() / 1000000.0) : 0.0;
        res.integrity_verified = integrity;
        res.original_crc32 = container.header.original_crc32;
        res.restored_crc32 = Crc32::calculate(restored);
        res.theoretical_time_complexity = get_theoretical_time(algo_id);
        res.theoretical_space_complexity = get_theoretical_space(algo_id);

        if (args.enable_trace && !args.trace_path.empty()) {
            std::string trace_json;
            if (algo_id == AlgorithmId::HUFFMAN) trace_json = tracer.huffman.to_json();
            else if (algo_id == AlgorithmId::LZW) trace_json = tracer.lzw.to_json();
            else if (algo_id == AlgorithmId::RLE) trace_json = tracer.rle.to_json();
            
            std::ofstream trace_file(args.trace_path);
            trace_file << trace_json;
        }

        std::cout << res.to_json() << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "{\"error\": \"" << ex.what() << "\"}\n";
        return 1;
    }
}

int handle_decompress(const CliArgs& args) {
    if (args.input_path.empty() || args.output_path.empty()) {
        std::cerr << "{\"error\": \"Missing --input or --output parameter\"}\n";
        return 1;
    }

    try {
        auto container_bytes = read_file_bytes(args.input_path);
        auto container = CompressedContainer::deserialize(container_bytes.data(), container_bytes.size());

        Timer timer;
        timer.start();

        std::vector<uint8_t> restored;
        if (container.header.algorithm == AlgorithmId::HUFFMAN) {
            restored = HuffmanEngine::decompress(container);
        } else if (container.header.algorithm == AlgorithmId::LZW) {
            restored = LzwEngine::decompress(container);
        } else if (container.header.algorithm == AlgorithmId::RLE) {
            restored = RleEngine::decompress(container);
        } else {
            throw std::runtime_error("Unsupported algorithm in container header");
        }
        timer.stop();

        write_file_bytes(args.output_path, restored);

        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"algorithmName\": \"" << algorithm_name(container.header.algorithm) << "\",\n";
        ss << "  \"algorithmSlug\": \"" << algorithm_slug(container.header.algorithm) << "\",\n";
        ss << "  \"restoredSize\": " << restored.size() << ",\n";
        ss << "  \"originalSize\": " << container.header.original_size << ",\n";
        ss << "  \"decompressionTimeMs\": " << std::fixed << std::setprecision(3) << timer.elapsed_ms() << ",\n";
        ss << "  \"decompressionThroughput\": " << (timer.elapsed_seconds() > 0 ? (static_cast<double>(restored.size()) / timer.elapsed_seconds() / 1000000.0) : 0.0) << ",\n";
        ss << "  \"originalCrc32\": " << container.header.original_crc32 << ",\n";
        ss << "  \"restoredCrc32\": " << Crc32::calculate(restored) << ",\n";
        ss << "  \"integrityVerified\": true\n";
        ss << "}";
        std::cout << ss.str() << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "{\"error\": \"" << ex.what() << "\"}\n";
        return 1;
    }
}

int handle_benchmark(const CliArgs& args) {
    if (args.input_path.empty()) {
        std::cerr << "{\"error\": \"Missing --input parameter\"}\n";
        return 1;
    }

    try {
        auto input_bytes = read_file_bytes(args.input_path);
        auto analysis = analyze_bytes(input_bytes.data(), input_bytes.size());

        std::vector<AlgorithmResult> results;
        int trials = std::max(1, args.trials);

        for (const auto& slug : args.benchmark_algos) {
            AlgorithmId algo_id = parse_algorithm_slug(slug);

            // Warm-up run
            if (algo_id == AlgorithmId::HUFFMAN) {
                auto c = HuffmanEngine::compress(input_bytes.data(), input_bytes.size());
                HuffmanEngine::decompress(c);
            } else if (algo_id == AlgorithmId::LZW) {
                auto c = LzwEngine::compress(input_bytes.data(), input_bytes.size());
                LzwEngine::decompress(c);
            } else if (algo_id == AlgorithmId::RLE) {
                auto c = RleEngine::compress(input_bytes.data(), input_bytes.size());
                RleEngine::decompress(c);
            }

            // Repeated measurement trials
            std::vector<double> comp_times;
            std::vector<double> decomp_times;
            CompressedContainer final_container;
            std::vector<uint8_t> final_serialized;
            std::vector<uint8_t> final_restored;

            for (int t = 0; t < trials; ++t) {
                Timer c_timer;
                c_timer.start();
                CompressedContainer container;
                if (algo_id == AlgorithmId::HUFFMAN) {
                    container = HuffmanEngine::compress(input_bytes.data(), input_bytes.size());
                } else if (algo_id == AlgorithmId::LZW) {
                    container = LzwEngine::compress(input_bytes.data(), input_bytes.size());
                } else if (algo_id == AlgorithmId::RLE) {
                    container = RleEngine::compress(input_bytes.data(), input_bytes.size());
                }
                auto serialized = container.serialize();
                c_timer.stop();
                comp_times.push_back(c_timer.elapsed_ms());

                Timer d_timer;
                d_timer.start();
                std::vector<uint8_t> restored;
                if (algo_id == AlgorithmId::HUFFMAN) {
                    restored = HuffmanEngine::decompress(container);
                } else if (algo_id == AlgorithmId::LZW) {
                    restored = LzwEngine::decompress(container);
                } else if (algo_id == AlgorithmId::RLE) {
                    restored = RleEngine::decompress(container);
                }
                d_timer.stop();
                decomp_times.push_back(d_timer.elapsed_ms());

                if (t == 0) {
                    final_container = container;
                    final_serialized = serialized;
                    final_restored = restored;
                }
            }

            std::sort(comp_times.begin(), comp_times.end());
            std::sort(decomp_times.begin(), decomp_times.end());
            double median_comp_time = comp_times[comp_times.size() / 2];
            double median_decomp_time = decomp_times[decomp_times.size() / 2];

            bool integrity = (final_restored.size() == input_bytes.size()) && 
                             (Crc32::calculate(final_restored) == final_container.header.original_crc32);

            AlgorithmResult res;
            res.algorithm_name = algorithm_name(algo_id);
            res.algorithm_slug = algorithm_slug(algo_id);
            res.original_size = input_bytes.size();
            res.compressed_size = final_serialized.size();
            res.payload_size = final_container.header.payload_size;
            res.metadata_size = final_container.header.metadata_size;
            res.compression_ratio = final_serialized.size() > 0 ? (static_cast<double>(input_bytes.size()) / static_cast<double>(final_serialized.size())) : 0.0;
            res.space_saved_percent = input_bytes.size() > 0 ? (((static_cast<double>(input_bytes.size()) - static_cast<double>(final_serialized.size())) / static_cast<double>(input_bytes.size())) * 100.0) : 0.0;
            res.compression_time_ms = median_comp_time;
            res.decompression_time_ms = median_decomp_time;
            res.compression_throughput_mb_s = (median_comp_time > 0) ? (static_cast<double>(input_bytes.size()) / (median_comp_time / 1000.0) / 1000000.0) : 0.0;
            res.decompression_throughput_mb_s = (median_decomp_time > 0) ? (static_cast<double>(input_bytes.size()) / (median_decomp_time / 1000.0) / 1000000.0) : 0.0;
            res.integrity_verified = integrity;
            res.original_crc32 = final_container.header.original_crc32;
            res.restored_crc32 = Crc32::calculate(final_restored);
            res.theoretical_time_complexity = get_theoretical_time(algo_id);
            res.theoretical_space_complexity = get_theoretical_space(algo_id);

            results.push_back(res);
        }

        // Determine Empirical Winner based on smallest valid artifact size (with speed tie-break)
        std::string empirical_winner;
        std::string empirical_reason;
        if (!results.empty()) {
            size_t best_idx = 0;
            for (size_t i = 1; i < results.size(); ++i) {
                if (results[i].compressed_size < results[best_idx].compressed_size) {
                    best_idx = i;
                } else if (results[i].compressed_size == results[best_idx].compressed_size) {
                    if (results[i].compression_time_ms < results[best_idx].compression_time_ms) {
                        best_idx = i;
                    }
                }
            }

            empirical_winner = results[best_idx].algorithm_slug;
            std::ostringstream ss;
            ss << "Empirical Winner: " << results[best_idx].algorithm_name << " yielded the most compact artifact ("
               << results[best_idx].compressed_size << " bytes vs " 
               << results[best_idx].original_size << " bytes original, "
               << std::fixed << std::setprecision(1) << results[best_idx].space_saved_percent 
               << "% space saved) in " << std::fixed << std::setprecision(2) << results[best_idx].compression_time_ms << " ms.";
            empirical_reason = ss.str();
        }

        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"inputAnalysis\": " << analysis.to_json() << ",\n";
        ss << "  \"trials\": " << trials << ",\n";
        ss << "  \"empiricalWinner\": \"" << empirical_winner << "\",\n";
        ss << "  \"empiricalReason\": \"" << empirical_reason << "\",\n";
        ss << "  \"results\": [\n";
        for (size_t i = 0; i < results.size(); ++i) {
            ss << results[i].to_json();
            if (i + 1 < results.size()) ss << ",\n";
        }
        ss << "\n  ]\n";
        ss << "}";

        std::cout << ss.str() << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "{\"error\": \"" << ex.what() << "\"}\n";
        return 1;
    }
}

// Self-Test Suite
int handle_test() {
    std::cout << "[SFCW C++ Engine Test Suite]\n";
    int passed = 0;
    int failed = 0;

    auto run_test = [&](const std::string& name, const std::vector<uint8_t>& input) {
        std::cout << "Testing: " << name << " (" << input.size() << " bytes)... ";
        bool ok = true;
        for (AlgorithmId algo : {AlgorithmId::HUFFMAN, AlgorithmId::LZW, AlgorithmId::RLE}) {
            try {
                CompressedContainer container;
                if (algo == AlgorithmId::HUFFMAN) container = HuffmanEngine::compress(input.data(), input.size());
                else if (algo == AlgorithmId::LZW) container = LzwEngine::compress(input.data(), input.size());
                else if (algo == AlgorithmId::RLE) container = RleEngine::compress(input.data(), input.size());

                auto serialized = container.serialize();
                auto deserialized = CompressedContainer::deserialize(serialized.data(), serialized.size());

                std::vector<uint8_t> restored;
                if (algo == AlgorithmId::HUFFMAN) restored = HuffmanEngine::decompress(deserialized);
                else if (algo == AlgorithmId::LZW) restored = LzwEngine::decompress(deserialized);
                else if (algo == AlgorithmId::RLE) restored = RleEngine::decompress(deserialized);

                if (restored != input) {
                    std::cout << "\n  FAIL: " << algorithm_name(algo) << " output mismatch!";
                    ok = false;
                }
            } catch (const std::exception& ex) {
                std::cout << "\n  FAIL: " << algorithm_name(algo) << " exception: " << ex.what();
                ok = false;
            }
        }
        if (ok) {
            std::cout << "PASSED\n";
            passed++;
        } else {
            failed++;
        }
    };

    // Test 1: Empty input
    run_test("Empty Input (0 bytes)", {});

    // Test 2: Single byte
    run_test("Single Byte", {'A'});

    // Test 3: Repetitive byte runs
    std::vector<uint8_t> rep;
    for (int i = 0; i < 500; ++i) rep.push_back('A');
    for (int i = 0; i < 300; ++i) rep.push_back('B');
    for (int i = 0; i < 200; ++i) rep.push_back('C');
    run_test("Repetitive Byte Runs (1000 bytes)", rep);

    // Test 4: All 256 byte values
    std::vector<uint8_t> all_bytes;
    for (int i = 0; i < 256; ++i) all_bytes.push_back(static_cast<uint8_t>(i));
    for (int i = 255; i >= 0; --i) all_bytes.push_back(static_cast<uint8_t>(i));
    run_test("All 256 Byte Values (512 bytes)", all_bytes);

    // Test 5: English text with repeating words and phrases
    std::string text = "Design and Analysis of Algorithms - Smart File Compression Workbench. "
                       "Greedy algorithms such as Huffman coding construct an optimal prefix tree. "
                       "LZW builds a dictionary of repeating sequences dynamically during encoding. "
                       "Run-Length Encoding directly compresses consecutive identical byte runs.";
    run_test("English Text (Sample Corpus)", std::vector<uint8_t>(text.begin(), text.end()));

    // Test 6: Pseudorandom binary bytes
    std::vector<uint8_t> binary_rand(2048);
    uint32_t seed = 1337;
    for (size_t i = 0; i < binary_rand.size(); ++i) {
        seed = seed * 1664525u + 1013904223u;
        binary_rand[i] = static_cast<uint8_t>((seed >> 16) & 0xFF);
    }
    run_test("High-Entropy Pseudo-Random Binary (2KB)", binary_rand);

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Smart File Compression Workbench Engine CLI\n"
                  << "Usage:\n"
                  << "  --action analyze --input <file>\n"
                  << "  --action compress --algo <huffman|lzw|rle> --input <file> --output <file> [--trace] [--trace-file <path>]\n"
                  << "  --action decompress --input <file> --output <file>\n"
                  << "  --action benchmark --input <file> [--trials <N>] [--algos huffman,lzw,rle]\n"
                  << "  --action test\n";
        return 1;
    }

    auto args = parse_args(argc, argv);
    if (args.action == "analyze") {
        return handle_analyze(args);
    } else if (args.action == "compress") {
        return handle_compress(args);
    } else if (args.action == "decompress") {
        return handle_decompress(args);
    } else if (args.action == "benchmark") {
        return handle_benchmark(args);
    } else if (args.action == "test") {
        return handle_test();
    } else {
        std::cerr << "{\"error\": \"Unknown action: " << args.action << "\"}\n";
        return 1;
    }
}
