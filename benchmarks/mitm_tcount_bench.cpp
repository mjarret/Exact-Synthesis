#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "config/Globals.hpp"
#include "ds/MITM.hpp"
#include "so6/SO6.hpp"

namespace fs = std::filesystem;

namespace {

constexpr double kMinSecondsPerBench = 2.0;
constexpr int kDefaultSamples = 10;

struct BenchArgs {
    fs::path bench_dir{"benchset_amy_random"};
    int samples{kDefaultSamples}; // 0 -> all
    int threads{0};
};

struct Bucket {
    int tcount{0};
    std::vector<SO6> targets;
};

std::vector<Bucket> g_buckets;
int g_threads = 0;

bool parse_nonnegative_int(const char* s, int& out) {
    if (!s || *s == '\0') return false;
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (!end || *end != '\0') return false;
    out = static_cast<int>(std::max<long>(0, v));
    return true;
}

BenchArgs parse_bench_args(int& argc, char** argv) {
    BenchArgs args;
    if (const char* env = std::getenv("BENCH_DIR")) {
        args.bench_dir = env;
    }
    if (const char* env = std::getenv("BENCH_SAMPLES")) {
        parse_nonnegative_int(env, args.samples);
    }
    if (const char* env = std::getenv("BENCH_THREADS")) {
        parse_nonnegative_int(env, args.threads);
    }

    int write = 1;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strncmp(a, "--bench-dir=", 12) == 0) {
            args.bench_dir = a + 12;
            continue;
        }
        if (std::strcmp(a, "--bench-dir") == 0 && i + 1 < argc) {
            args.bench_dir = argv[++i];
            continue;
        }
        if (std::strncmp(a, "--samples=", 10) == 0) {
            parse_nonnegative_int(a + 10, args.samples);
            continue;
        }
        if (std::strcmp(a, "--samples") == 0 && i + 1 < argc) {
            parse_nonnegative_int(argv[++i], args.samples);
            continue;
        }
        if (std::strncmp(a, "--threads=", 10) == 0) {
            parse_nonnegative_int(a + 10, args.threads);
            continue;
        }
        if (std::strcmp(a, "--threads") == 0 && i + 1 < argc) {
            parse_nonnegative_int(argv[++i], args.threads);
            continue;
        }
        argv[write++] = argv[i];
    }
    argc = write;
    return args;
}

std::vector<std::string> load_labels(const fs::path& labels_path) {
    std::ifstream in(labels_path);
    if (!in) return {};
    std::vector<std::string> labels;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) labels.push_back(line);
    }
    return labels;
}

std::vector<std::string> list_labels_from_targets(const fs::path& targets_dir) {
    std::vector<std::string> labels;
    for (const auto& entry : fs::directory_iterator(targets_dir)) {
        if (!entry.is_regular_file()) continue;
        const fs::path& p = entry.path();
        if (p.extension() == ".mat") {
            labels.push_back(p.stem().string());
        }
    }
    std::sort(labels.begin(), labels.end());
    return labels;
}

SO6 load_target(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("missing target: " + path.string());
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    SO6 target(text);
    target.last_T = 15;
    return target;
}

int parse_tcount_dir(const fs::path& dir) {
    const std::string name = dir.filename().string();
    if (name.rfind("tcount_", 0) != 0) return -1;
    const std::string num = name.substr(std::strlen("tcount_"));
    if (num.empty()) return -1;
    char* end = nullptr;
    long v = std::strtol(num.c_str(), &end, 10);
    if (!end || *end != '\0') return -1;
    return static_cast<int>(v);
}

std::vector<Bucket> load_buckets(const fs::path& bench_dir, int samples) {
    std::vector<Bucket> buckets;
    if (!fs::exists(bench_dir) || !fs::is_directory(bench_dir)) {
        throw std::runtime_error("bench dir not found: " + bench_dir.string());
    }

    for (const auto& entry : fs::directory_iterator(bench_dir)) {
        if (!entry.is_directory()) continue;
        const fs::path bucket_dir = entry.path();
        const int tcount = parse_tcount_dir(bucket_dir);
        if (tcount < 0) continue;

        const fs::path targets_dir = bucket_dir / "targets";
        if (!fs::exists(targets_dir)) continue;

        std::vector<std::string> labels = load_labels(bucket_dir / "labels.txt");
        if (labels.empty()) {
            labels = list_labels_from_targets(targets_dir);
        }
        if (labels.empty()) continue;

        std::vector<std::string> selected;
        if (samples == 0) {
            selected = labels;
        } else {
            const int target_samples = std::max(1, samples);
            std::mt19937_64 rng(1337u + static_cast<uint64_t>(tcount));
            if (static_cast<int>(labels.size()) >= target_samples) {
                std::shuffle(labels.begin(), labels.end(), rng);
                selected.assign(labels.begin(), labels.begin() + target_samples);
            } else {
                selected.reserve(static_cast<size_t>(target_samples));
                std::uniform_int_distribution<size_t> pick(0, labels.size() - 1);
                for (int i = 0; i < target_samples; ++i) {
                    selected.push_back(labels[pick(rng)]);
                }
            }
        }

        Bucket bucket;
        bucket.tcount = tcount;
        bucket.targets.reserve(selected.size());
        for (const auto& label : selected) {
            bucket.targets.push_back(load_target(targets_dir / (label + ".mat")));
        }
        if (!bucket.targets.empty()) {
            buckets.push_back(std::move(bucket));
        }
    }

    std::sort(buckets.begin(), buckets.end(),
              [](const Bucket& a, const Bucket& b) { return a.tcount < b.tcount; });
    return buckets;
}

void configure_globals_for_tcount(int tcount, int threads, bool use_bf) {
    target_T_count = static_cast<uint8_t>(std::max(1, tcount));
    stored_depth_max = static_cast<uint8_t>(std::max(0, tcount - 1));
    THREADS = threads > 0 ? static_cast<uint8_t>(threads)
                          : static_cast<uint8_t>(std::max(1u, std::thread::hardware_concurrency()));
    verbose = false;
    suppress_indicators = true;
    mitm_bf_extension = use_bf;
    Globals::configure();
}

void run_bucket(benchmark::State& state, const Bucket* bucket, bool use_bf) {
    configure_globals_for_tcount(bucket->tcount, g_threads, use_bf);
    for (auto _ : state) {
        for (const auto& target : bucket->targets) {
            MITM mitm(SO6::identity(), target);
            MITMMatchResult res = generate_mitm_match(mitm);
            benchmark::DoNotOptimize(res.found);
            benchmark::ClobberMemory();
        }
    }
    state.counters["tcount"] = bucket->tcount;
    state.counters["samples"] = static_cast<double>(bucket->targets.size());
}

} // namespace

int main(int argc, char** argv) {
    BenchArgs args = parse_bench_args(argc, argv);

    try {
        g_buckets = load_buckets(args.bench_dir, args.samples);
    } catch (const std::exception& e) {
        std::cerr << "mitm_tcount_bench: " << e.what() << "\n";
        return 2;
    }

    if (g_buckets.empty()) {
        std::cerr << "mitm_tcount_bench: no tcount buckets found in " << args.bench_dir << "\n";
        return 2;
    }

    g_threads = args.threads;

    benchmark::Initialize(&argc, argv);
    for (const auto& bucket : g_buckets) {
        if (bucket.tcount > 20) {
            continue;
        }
        const std::string base = "mitm/tcount_" + std::to_string(bucket.tcount);
        const std::string std_name = base + "/standard";
        const std::string bf_name = base + "/bf";

        if (bucket.tcount <= 18) {
            benchmark::RegisterBenchmark(std_name.c_str(),
                                         [&bucket](benchmark::State& state) {
                                             run_bucket(state, &bucket, false);
                                         })
                ->MinTime(kMinSecondsPerBench)
                ->Unit(benchmark::kMillisecond);
        }
        benchmark::RegisterBenchmark(bf_name.c_str(),
                                     [&bucket](benchmark::State& state) {
                                         run_bucket(state, &bucket, true);
                                     })
            ->MinTime(kMinSecondsPerBench)
            ->Unit(benchmark::kMillisecond);
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
