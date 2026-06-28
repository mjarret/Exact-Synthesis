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
constexpr int kDefaultSamples = 30;

struct Sample {
    std::string label;
    SO6 target;
};

struct BenchArgs {
    fs::path bench_dir{"benchset_amy_random"};
    fs::path mitms_bin{"mitms/mitms"};
    int tcount{-1};
    int samples{kDefaultSamples};
    int mitms_threads{0};
};

struct BenchConfig {
    fs::path bucket_dir;
    fs::path mitms_bin;
    fs::path mitms_dir;
    int tcount{0};
    int mitms_threads{1};
    std::vector<Sample> samples;
};

BenchConfig g_cfg;

bool parse_positive_int(const char* s, int& out) {
    if (!s || *s == '\0') return false;
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (!end || *end != '\0') return false;
    out = static_cast<int>(std::max<long>(1, v));
    return true;
}

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
    if (const char* env = std::getenv("MITMS_BIN")) {
        args.mitms_bin = env;
    }
    if (const char* env = std::getenv("BENCH_SAMPLES")) {
        parse_positive_int(env, args.samples);
    }

    int write = 1;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strncmp(a, "--tcount=", 9) == 0) {
            parse_nonnegative_int(a + 9, args.tcount);
            continue;
        }
        if (std::strcmp(a, "--tcount") == 0 && i + 1 < argc) {
            parse_nonnegative_int(argv[++i], args.tcount);
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
        if (std::strncmp(a, "--mitms-bin=", 12) == 0) {
            args.mitms_bin = a + 12;
            continue;
        }
        if (std::strcmp(a, "--mitms-bin") == 0 && i + 1 < argc) {
            args.mitms_bin = argv[++i];
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
        if (std::strncmp(a, "--mitms-threads=", 16) == 0) {
            parse_positive_int(a + 16, args.mitms_threads);
            continue;
        }
        if (std::strcmp(a, "--mitms-threads") == 0 && i + 1 < argc) {
            parse_positive_int(argv[++i], args.mitms_threads);
            continue;
        }
        argv[write++] = argv[i];
    }
    argc = write;
    return args;
}

void print_usage() {
    std::cerr
        << "Usage: mitm_vs_mitms_bench --tcount=N [--bench-dir=DIR] [--samples=N]\n"
        << "       [--mitms-threads=N]\n"
        << "Env: BENCH_DIR, MITMS_BIN, BENCH_SAMPLES\n";
}

void configure_globals(int tcount) {
    target_T_count = static_cast<uint8_t>(std::max(1, tcount));
    stored_depth_max = static_cast<uint8_t>(std::max(0, tcount - 1));
    THREADS = std::thread::hardware_concurrency();
    verbose = false;
    suppress_indicators = true;
    Globals::configure();
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

struct ScopedCwd {
    fs::path saved;
    explicit ScopedCwd(const fs::path& next) : saved(fs::current_path()) {
        fs::current_path(next);
    }
    ~ScopedCwd() { fs::current_path(saved); }
};

int run_mitms_once(const fs::path& mitms_bin, const std::string& label) {
    std::string cmd = mitms_bin.string() + " -no-serialize -threads 1 " + label +
                      " > /dev/null 2>&1";
    return std::system(cmd.c_str());
}

std::string shell_quote(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'') {
            out.append("'\\''");
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

fs::path make_temp_path(const char* stem) {
    auto base = fs::temp_directory_path();
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream name;
    name << stem << "_" << now << "_" << std::rand() << ".txt";
    return base / name.str();
}

bool validate_mitms_label(const fs::path& mitms_bin,
                          const fs::path& mitms_dir,
                          const std::string& label,
                          std::string& out_err) {
    fs::path tmp = make_temp_path("mitms_bench");
    {
std::string cmd = "cd " + shell_quote(mitms_dir.string()) + " && " +
                          shell_quote(mitms_bin.string()) + " -no-serialize -threads " +
                          std::to_string(g_cfg.mitms_threads) + " " +
                          shell_quote(label) + " > " + shell_quote(tmp.string()) + " 2>&1";
        int rc = std::system(cmd.c_str());
        if (rc != 0) {
            std::ifstream in(tmp);
            out_err.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            fs::remove(tmp);
            return false;
        }
    }
    std::ifstream in(tmp);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    fs::remove(tmp);
    if (text.find("No circuit") != std::string::npos) { out_err = text; return false; }
    if (text.find("ERROR:") != std::string::npos) { out_err = text; return false; }
    return true;
}

static void BM_MITM_Ours(benchmark::State& state) {
    for (auto _ : state) {
        for (const auto& sample : g_cfg.samples) {
            MITM mitm(SO6::identity(), sample.target);
            MITMMatchResult res = generate_mitm_match(mitm);
            benchmark::DoNotOptimize(res.found);
            benchmark::ClobberMemory();
        }
    }
}

static void BM_MITM_Mitms(benchmark::State& state) {
    for (auto _ : state) {
        for (const auto& sample : g_cfg.samples) {
            std::string cmd = "cd " + shell_quote(g_cfg.mitms_dir.string()) + " && " +
                              shell_quote(g_cfg.mitms_bin.string()) +
                              " -no-serialize -threads " +
                              std::to_string(g_cfg.mitms_threads) + " " +
                              shell_quote(sample.label) + " > /dev/null 2>&1";
            int rc = std::system(cmd.c_str());
            benchmark::DoNotOptimize(rc);
            benchmark::ClobberMemory();
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    BenchArgs args = parse_bench_args(argc, argv);
    if (args.tcount < 0) {
        print_usage();
        return 2;
    }

    fs::path bucket_dir = args.bench_dir;
    if (!fs::exists(bucket_dir / "labels.txt")) {
        bucket_dir = args.bench_dir / ("tcount_" + std::to_string(args.tcount));
    }
    fs::path labels_path = bucket_dir / "labels.txt";
    fs::path targets_dir = bucket_dir / "targets";
    fs::path searches_path = bucket_dir / "searches";
    if (!fs::exists(labels_path)) {
        std::cerr << "missing labels.txt in " << bucket_dir << "\n";
        return 2;
    }
    if (!fs::exists(searches_path)) {
        std::cerr << "missing searches in " << bucket_dir << "\n";
        return 2;
    }
    if (!fs::exists(targets_dir)) {
        std::cerr << "missing targets/ in " << bucket_dir << "\n";
        return 2;
    }

    std::vector<std::string> labels = load_labels(labels_path);
    if (labels.empty()) {
        std::cerr << "no labels found in " << labels_path << "\n";
        return 2;
    }
    if (args.samples > static_cast<int>(labels.size())) {
        args.samples = static_cast<int>(labels.size());
    }

    std::mt19937_64 rng(1337);
    std::shuffle(labels.begin(), labels.end(), rng);

    g_cfg.bucket_dir = bucket_dir;
    g_cfg.mitms_bin = fs::absolute(args.mitms_bin);
    g_cfg.mitms_dir = g_cfg.mitms_bin.parent_path();
    g_cfg.tcount = args.tcount;

    if (!fs::exists(g_cfg.mitms_bin)) {
        std::cerr << "missing mitms binary: " << g_cfg.mitms_bin << "\n";
        return 2;
    }
    fs::path mitms_searches = g_cfg.mitms_dir / "searches";
    fs::copy_file(searches_path, mitms_searches, fs::copy_options::overwrite_existing);
    std::vector<Sample> samples;
    samples.reserve(static_cast<size_t>(args.samples));
    for (const auto& label : labels) {
        if (static_cast<int>(samples.size()) >= args.samples) break;
        std::string err;
        if (!validate_mitms_label(g_cfg.mitms_bin, g_cfg.mitms_dir, label, err)) {
            std::cerr << "mitms failed for label " << label << "\n";
            if (!err.empty()) {
                std::cerr << err.substr(0, 200) << "\n";
            }
            continue;
        }
        fs::path mat_path = targets_dir / (label + ".mat");
        samples.push_back({label, load_target(mat_path)});
    }
    if (samples.size() < static_cast<size_t>(args.samples)) {
        std::cerr << "not enough mitms-compatible labels: got " << samples.size()
                  << " expected " << args.samples << "\n";
        return 2;
    }
    g_cfg.samples = std::move(samples);

    configure_globals(g_cfg.tcount);
    g_cfg.mitms_threads = args.mitms_threads > 0
        ? args.mitms_threads
        : static_cast<int>(std::max<int>(1, THREADS));

    benchmark::Initialize(&argc, argv);
    const std::string ours_name = "mitm/ours/tcount_" + std::to_string(g_cfg.tcount);
    const std::string mitms_name = "mitm/mitms/tcount_" + std::to_string(g_cfg.tcount);
    benchmark::RegisterBenchmark(ours_name.c_str(), &BM_MITM_Ours)
        ->MinTime(kMinSecondsPerBench)
        ->Unit(benchmark::kMillisecond);
    benchmark::RegisterBenchmark(mitms_name.c_str(), &BM_MITM_Mitms)
        ->MinTime(kMinSecondsPerBench)
        ->Unit(benchmark::kMillisecond);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
