#include <benchmark/benchmark.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "sys/memory.hpp"
#include "mitms/src/circuit.h"
#include "mitms/src/configs.h"
#include "mitms/src/gate.h"
#include "mitms/src/matrix.h"
#include "mitms/src/ring.h"
#include "mitms/src/search.h"
#include "mitms/src/util.h"

namespace fs = std::filesystem;

namespace {

constexpr int kDefaultSamples = 10;
constexpr int kMinSamples = 3;
constexpr int kMaxSamplesMultiplier = 5;
constexpr int kMaxSamplesCap = 100;
constexpr double kCvThreshold = 0.10;

struct Bucket {
    int tcount{0};
    fs::path searches_path;
    int qubits{0};
    std::vector<std::string> labels;
    std::vector<Rmatrix> targets;
    uint64_t seed{0};
    int iterations{0};
    double cv{0.0};
    double mean_seconds{0.0};
};

struct BenchArgs {
    fs::path bench_dir{"tmp_out"};
    int tcount{-1};
    int mitms_threads{0};
    int samples{kDefaultSamples};
    int iterations{0};
    bool verbose{false};
    bool progress{true};
};

int g_threads = 1;
bool g_verbose = false;
bool g_progress = true;
std::vector<Bucket> g_buckets;

bool parse_positive_int(const char* s, int& out) {
    if (!s || *s == '\0') return false;
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (!end || *end != '\0') return false;
    out = static_cast<int>(std::max<long>(1, v));
    return true;
}

BenchArgs parse_bench_args(int& argc, char** argv) {
    BenchArgs args;
    if (const char* env = std::getenv("BENCH_DIR")) {
        args.bench_dir = env;
    }
    if (const char* env = std::getenv("BENCH_SAMPLES")) {
        parse_positive_int(env, args.samples);
    }
    if (const char* env = std::getenv("BENCH_ITERATIONS")) {
        parse_positive_int(env, args.iterations);
    }

    int write = 1;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strncmp(a, "--tcount=", 9) == 0) {
            parse_positive_int(a + 9, args.tcount);
            continue;
        }
        if (std::strcmp(a, "--tcount") == 0 && i + 1 < argc) {
            parse_positive_int(argv[++i], args.tcount);
            continue;
        }
        if (std::strncmp(a, "--bench-dir=", 12) == 0) {
            args.bench_dir = a + 12;
            continue;
        }
        if (std::strcmp(a, "--bench-dir") == 0 && i + 1 < argc) {
            args.bench_dir = argv[++i];
            continue;
        }
        if (std::strncmp(a, "--mitms-threads=", 16) == 0) {
            parse_positive_int(a + 16, args.mitms_threads);
            continue;
        }
        if (std::strcmp(a, "--mitms-threads") == 0 && i + 1 < argc) {
            parse_positive_int(argv[++i], args.mitms_threads);
            continue;
        }
        if (std::strncmp(a, "--samples=", 10) == 0) {
            parse_positive_int(a + 10, args.samples);
            continue;
        }
        if (std::strcmp(a, "--samples") == 0 && i + 1 < argc) {
            parse_positive_int(argv[++i], args.samples);
            continue;
        }
        if (std::strncmp(a, "--iterations=", 13) == 0) {
            parse_positive_int(a + 13, args.iterations);
            continue;
        }
        if (std::strcmp(a, "--iterations") == 0 && i + 1 < argc) {
            parse_positive_int(argv[++i], args.iterations);
            continue;
        }
        if (std::strcmp(a, "--verbose") == 0) {
            args.verbose = true;
            continue;
        }
        if (std::strncmp(a, "--verbose=", 10) == 0) {
            args.verbose = std::strcmp(a + 10, "0") != 0;
            continue;
        }
        if (std::strcmp(a, "--no-progress") == 0) {
            args.progress = false;
            continue;
        }
        if (std::strncmp(a, "--progress=", 11) == 0) {
            args.progress = std::strcmp(a + 11, "0") != 0;
            continue;
        }
        argv[write++] = argv[i];
    }
    argc = write;
    return args;
}

void print_usage() {
    std::cerr
        << "Usage: mitms_searches_bench [--tcount=N] [--bench-dir=DIR]\n"
        << "       [--mitms-threads=N] [--samples=N] [--iterations=N]\n"
        << "       [--verbose] [--no-progress]\n"
        << "Env: BENCH_DIR, BENCH_SAMPLES, BENCH_ITERATIONS\n";
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

struct SearchEntry {
    std::string label;
    int qubits{0};
    std::vector<std::string> rows;
};

std::vector<SearchEntry> load_search_entries(const fs::path& searches_path) {
    std::ifstream in(searches_path);
    if (!in) {
        throw std::runtime_error("missing searches: " + searches_path.string());
    }
    std::vector<SearchEntry> entries;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream header(line);
        SearchEntry entry;
        if (!(header >> entry.label >> entry.qubits) || entry.qubits <= 0) {
            throw std::runtime_error("invalid entry in searches: " + searches_path.string());
        }
        entry.rows.reserve(static_cast<size_t>(entry.qubits));
        for (int q = 0; q < entry.qubits; ++q) {
            std::string row;
            if (!std::getline(in, row)) {
                throw std::runtime_error("incomplete circuit rows in searches: " + searches_path.string());
            }
            entry.rows.push_back(std::move(row));
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

class NullBuffer : public std::streambuf {
public:
    int overflow(int c) override { return c; }
};

class ScopedMuteCout {
public:
    explicit ScopedMuteCout(bool enabled = true) : prev_(nullptr) {
        if (enabled) {
            prev_ = std::cout.rdbuf(&null_);
        }
    }
    ~ScopedMuteCout() {
        if (prev_) {
            std::cout.rdbuf(prev_);
        }
    }

private:
    std::streambuf* prev_;
    NullBuffer null_;
};

void configure_mitms(int qubits, int threads, int& configured_qubits) {
    if (configured_qubits == -1) {
        init_configs(qubits);
        init_ring();
        init_rmatrix();
        init_gate();
        init_util();
        configured_qubits = qubits;
    } else if (configured_qubits != qubits) {
        throw std::runtime_error("mixed qubit counts in searches are unsupported");
    }

    config::num_threads = threads;
    config::serialize = false;
    config::approximate = false;
    config::tdepth = false;
    config::mod_phase = true;
    config::mod_perms = true;
    config::mod_invs = true;
}

Rmatrix build_target_matrix(const SearchEntry& entry) {
    std::ostringstream ss;
    for (const auto& row : entry.rows) {
        ss << row << "\n";
    }
    std::istringstream in(ss.str());
    Circuit circ = read_circuit(in);
    Rmatrix V(dim, dim);
    circ.to_Rmatrix(V);
    Rmatrix U(dim_proj, dim_proj);
    V.submatrix(0, 0, dim_proj, dim_proj, U);
    delete_circuit(circ);
    return U;
}

struct CalibrationResult {
    int iterations{0};
    double cv{0.0};
    double mean_seconds{0.0};
};

CalibrationResult calibrate_bucket_iterations(const Bucket& bucket, int threads) {
    CalibrationResult result;
    if (bucket.targets.empty()) return result;

    ScopedMuteCout mute(!g_verbose);
    config::num_threads = threads;

    size_t idx = 0;
    int samples = 0;
    double mean = 0.0;
    double m2 = 0.0;
    double cv = 0.0;
    const int max_samples = std::min(
        kMaxSamplesCap,
        std::max(kMinSamples,
                 static_cast<int>(bucket.targets.size()) * kMaxSamplesMultiplier));

    while (samples < max_samples) {
        const size_t pick = idx % bucket.targets.size();
        Rmatrix target = bucket.targets[pick];
        const auto t0 = std::chrono::steady_clock::now();
        bool found = exact_search_first(target);
        const auto t1 = std::chrono::steady_clock::now();

        const double dt = std::chrono::duration<double>(t1 - t0).count();
        ++samples;
        const double delta = dt - mean;
        mean += delta / static_cast<double>(samples);
        const double delta2 = dt - mean;
        m2 += delta * delta2;

        benchmark::DoNotOptimize(found);
        benchmark::ClobberMemory();
        ++idx;

        if (samples >= kMinSamples) {
            const double variance = (samples > 1) ? (m2 / static_cast<double>(samples - 1)) : 0.0;
            const double stdev = variance > 0.0 ? std::sqrt(variance) : 0.0;
            cv = (mean > 0.0) ? (stdev / mean) : 0.0;
            if (cv <= kCvThreshold) {
                break;
            }
        }
    }

    result.iterations = samples;
    result.cv = cv;
    result.mean_seconds = mean;
    return result;
}

static void BM_MITMS_Samples(benchmark::State& state, Bucket* bucket) {
    ScopedMuteCout mute(!g_verbose);
    if (bucket->targets.empty()) {
        state.SkipWithError("no samples available");
        return;
    }
    size_t idx = 0;
    std::size_t max_rss = 0;
    std::size_t max_delta = 0;
    for (auto _ : state) {
        config::num_threads = g_threads;
        const size_t pick = idx % bucket->targets.size();
        const std::size_t rss_before = getProcessRSSBytes();
        Rmatrix target = bucket->targets[pick];
        bool found = exact_search_first(target);
        const std::size_t rss_after = getProcessRSSBytes();

        if (rss_after > 0) {
            max_rss = std::max(max_rss, rss_after);
        }
        if (rss_after > rss_before) {
            max_delta = std::max(max_delta, rss_after - rss_before);
        }

        benchmark::DoNotOptimize(found);
        benchmark::ClobberMemory();
        ++idx;
    }
    state.counters["cv"] = bucket->cv;
    state.counters["calib_mean_s"] = bucket->mean_seconds;
    state.counters["rss_bytes"] = static_cast<double>(max_rss);
    state.counters["rss_delta_bytes"] = static_cast<double>(max_delta);
}

Bucket load_bucket(const fs::path& bucket_dir,
                   int tcount,
                   int threads,
                   int samples,
                   int& configured_qubits) {
    const fs::path searches_path = bucket_dir / "searches";
    if (!fs::exists(searches_path)) {
        throw std::runtime_error("missing searches in " + bucket_dir.string());
    }

    std::vector<SearchEntry> entries = load_search_entries(searches_path);
    Bucket bucket;
    bucket.tcount = tcount;
    bucket.searches_path = searches_path;
    if (entries.empty()) {
        return bucket;
    }
    configure_mitms(entries.front().qubits, threads, configured_qubits);

    bucket.qubits = entries.front().qubits;

    for (const auto& entry : entries) {
        if (entry.qubits != bucket.qubits) {
            throw std::runtime_error("mixed qubit counts in searches: " + searches_path.string());
        }
    }

    std::random_device rd;
    bucket.seed = (static_cast<uint64_t>(rd()) << 32) ^ rd() ^ static_cast<uint64_t>(tcount);
    std::mt19937_64 rng(bucket.seed);
    std::vector<size_t> order;
    if (samples <= 0) samples = kDefaultSamples;
    order.resize(entries.size());
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    if (static_cast<int>(order.size()) > samples) {
        order.resize(static_cast<size_t>(samples));
    }

    bucket.labels.reserve(order.size());
    bucket.targets.reserve(order.size());
    for (size_t idx : order) {
        const auto& entry = entries[idx];
        bucket.labels.push_back(entry.label);
        bucket.targets.push_back(build_target_matrix(entry));
    }

    if (g_verbose) {
        std::cout << "mitms_searches_bench: tcount " << bucket.tcount
                  << " samples " << bucket.targets.size() << "\n";
    }
    return bucket;
}

std::vector<Bucket> load_buckets(const fs::path& bench_dir,
                                 int tcount_filter,
                                 int threads,
                                 int samples,
                                 int& configured_qubits) {
    std::vector<Bucket> buckets;

    if (fs::exists(bench_dir / "searches")) {
        int tcount = tcount_filter;
        if (tcount < 0) {
            tcount = parse_tcount_dir(bench_dir);
            if (tcount < 0) tcount = 0;
        }
        Bucket bucket = load_bucket(bench_dir, tcount, threads, samples, configured_qubits);
        if (!bucket.targets.empty()) {
            buckets.push_back(std::move(bucket));
        }
        return buckets;
    }

    if (tcount_filter >= 0) {
        fs::path bucket_dir = bench_dir / ("tcount_" + std::to_string(tcount_filter));
        Bucket bucket = load_bucket(bucket_dir, tcount_filter, threads, samples, configured_qubits);
        if (!bucket.targets.empty()) {
            buckets.push_back(std::move(bucket));
        }
        return buckets;
    }

    if (!fs::exists(bench_dir) || !fs::is_directory(bench_dir)) {
        throw std::runtime_error("bench dir not found: " + bench_dir.string());
    }

    for (const auto& entry : fs::directory_iterator(bench_dir)) {
        if (!entry.is_directory()) continue;
        const fs::path bucket_dir = entry.path();
        const int tcount = parse_tcount_dir(bucket_dir);
        if (tcount < 0) continue;
        try {
            Bucket bucket = load_bucket(bucket_dir, tcount, threads, samples, configured_qubits);
            if (!bucket.targets.empty()) {
                buckets.push_back(std::move(bucket));
            }
        } catch (const std::exception&) {
            continue;
        }
    }

    std::sort(buckets.begin(), buckets.end(),
              [](const Bucket& a, const Bucket& b) { return a.tcount < b.tcount; });
    return buckets;
}

} // namespace

int main(int argc, char** argv) {
    BenchArgs args = parse_bench_args(argc, argv);

    g_verbose = args.verbose;
    g_progress = args.progress;
    set_search_verbose(g_verbose);
    g_threads = args.mitms_threads > 0
        ? args.mitms_threads
        : static_cast<int>(std::max<int>(1, std::thread::hardware_concurrency()));

    int configured_qubits = -1;
    try {
        g_buckets = load_buckets(args.bench_dir,
                                 args.tcount,
                                 g_threads,
                                 args.samples,
                                 configured_qubits);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 2;
    }
    if (g_buckets.empty()) {
        std::cerr << "no usable tcount buckets found in " << args.bench_dir << "\n";
        return 0;
    }

    for (auto& bucket : g_buckets) {
        if (args.iterations > 0) {
            bucket.iterations = args.iterations;
            bucket.cv = 0.0;
            bucket.mean_seconds = 0.0;
            if (g_progress) {
                std::cerr << "[mitms_searches_bench] tcount " << bucket.tcount
                          << " using fixed iterations " << bucket.iterations << "\n";
            }
            continue;
        }

        if (g_progress) {
            std::cerr << "[mitms_searches_bench] calibrating tcount "
                      << bucket.tcount << "...\n";
        }
        CalibrationResult calib = calibrate_bucket_iterations(bucket, g_threads);
        bucket.iterations = std::max(kMinSamples, calib.iterations);
        bucket.cv = calib.cv;
        bucket.mean_seconds = calib.mean_seconds;
        if (g_progress) {
            std::cerr << "[mitms_searches_bench] calibrated tcount "
                      << bucket.tcount << " iterations " << bucket.iterations
                      << " cv " << bucket.cv << "\n";
        }
    }

    benchmark::Initialize(&argc, argv);
    for (auto& bucket : g_buckets) {
        const std::string bench_name =
            "mitms/depth/samples/tcount_" + std::to_string(bucket.tcount);
        benchmark::RegisterBenchmark(
            bench_name.c_str(),
            [&bucket](benchmark::State& state) {
                if (!bucket.labels.empty()) {
                    state.SetLabel("samples=" + std::to_string(bucket.labels.size()) +
                                   " iters=" + std::to_string(bucket.iterations));
                }
                BM_MITMS_Samples(state, &bucket);
            })
            ->Iterations(bucket.iterations)
            ->Unit(benchmark::kMillisecond);
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
