#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "config/Globals.hpp"
#include "ds/MITM.hpp"
#include "so6/SO6.hpp"
#include "util/amy_circuit.hpp"
#include <tbb/global_control.h>

namespace fs = std::filesystem;

namespace {

static volatile std::sig_atomic_t g_stop_requested = 0;

static void handle_stop_signal(int) {
    g_stop_requested = 1;
}

static bool stop_requested() {
    return g_stop_requested != 0;
}

static inline DyadicSqrt2 Z() { return DyadicSqrt2(0, 0, 0); }
static inline DyadicSqrt2 One() { return DyadicSqrt2(1, 0, 0); }
static inline DyadicSqrt2 NegOne() { return DyadicSqrt2(-1, 0, 0); }
static inline DyadicSqrt2 InvSqrt2() { return DyadicSqrt2(1, 0, 1); }
static inline DyadicSqrt2 NegInvSqrt2() { return DyadicSqrt2(-1, 0, 1); }

static SO6 from_rows(const std::array<std::array<DyadicSqrt2, 6>, 6>& M) {
    SO6 s;
    for (uint8_t r = 0; r < 6; ++r) {
        for (uint8_t c = 0; c < 6; ++c) {
            s.set_element(r, c, M[r][c]);
        }
    }
    return s;
}

// SO(6) images from the paper (Overleaf "two qubit compilation")
static const SO6& H0() { static const SO6 m = from_rows({{
    {{Z(), Z(), One(), Z(), Z(), Z()}},
    {{Z(), NegOne(), Z(), Z(), Z(), Z()}},
    {{One(), Z(), Z(), Z(), Z(), Z()}},
    {{Z(), Z(), Z(), One(), Z(), Z()}},
    {{Z(), Z(), Z(), Z(), One(), Z()}},
    {{Z(), Z(), Z(), Z(), Z(), One()}}
}}); return m; }

static const SO6& S0() { static const SO6 m = from_rows({{
    {{Z(), NegOne(), Z(), Z(), Z(), Z()}},
    {{One(), Z(), Z(), Z(), Z(), Z()}},
    {{Z(), Z(), One(), Z(), Z(), Z()}},
    {{Z(), Z(), Z(), One(), Z(), Z()}},
    {{Z(), Z(), Z(), Z(), One(), Z()}},
    {{Z(), Z(), Z(), Z(), Z(), One()}}
}}); return m; }

static const SO6& T0() { static const SO6 m = from_rows({{
    {{InvSqrt2(), NegInvSqrt2(), Z(), Z(), Z(), Z()}},
    {{InvSqrt2(), InvSqrt2(), Z(), Z(), Z(), Z()}},
    {{Z(), Z(), One(), Z(), Z(), Z()}},
    {{Z(), Z(), Z(), One(), Z(), Z()}},
    {{Z(), Z(), Z(), Z(), One(), Z()}},
    {{Z(), Z(), Z(), Z(), Z(), One()}}
}}); return m; }

static const SO6& H1() { static const SO6 m = from_rows({{
    {{One(), Z(), Z(), Z(), Z(), Z()}},
    {{Z(), One(), Z(), Z(), Z(), Z()}},
    {{Z(), Z(), One(), Z(), Z(), Z()}},
    {{Z(), Z(), Z(), Z(), Z(), One()}},
    {{Z(), Z(), Z(), Z(), NegOne(), Z()}},
    {{Z(), Z(), Z(), One(), Z(), Z()}}
}}); return m; }

static const SO6& S1() { static const SO6 m = from_rows({{
    {{One(), Z(), Z(), Z(), Z(), Z()}},
    {{Z(), One(), Z(), Z(), Z(), Z()}},
    {{Z(), Z(), One(), Z(), Z(), Z()}},
    {{Z(), Z(), Z(), Z(), NegOne(), Z()}},
    {{Z(), Z(), Z(), One(), Z(), Z()}},
    {{Z(), Z(), Z(), Z(), Z(), One()}}
}}); return m; }

static const SO6& T1() { static const SO6 m = from_rows({{
    {{One(), Z(), Z(), Z(), Z(), Z()}},
    {{Z(), One(), Z(), Z(), Z(), Z()}},
    {{Z(), Z(), One(), Z(), Z(), Z()}},
    {{Z(), Z(), Z(), InvSqrt2(), NegInvSqrt2(), Z()}},
    {{Z(), Z(), Z(), InvSqrt2(), InvSqrt2(), Z()}},
    {{Z(), Z(), Z(), Z(), Z(), One()}}
}}); return m; }

static const SO6& CZ() { static const SO6 m = from_rows({{
    {{Z(), NegOne(), Z(), Z(), Z(), Z()}},
    {{One(), Z(), Z(), Z(), Z(), Z()}},
    {{Z(), Z(), Z(), Z(), Z(), NegOne()}},
    {{Z(), Z(), Z(), Z(), NegOne(), Z()}},
    {{Z(), Z(), Z(), One(), Z(), Z()}},
    {{Z(), Z(), One(), Z(), Z(), Z()}}
}}); return m; }

static SO6 transpose(const SO6& A) {
    SO6 out;
    for (uint8_t r = 0; r < 6; ++r) {
        for (uint8_t c = 0; c < 6; ++c) {
            out.set_element(r, c, A.get_element(c, r));
        }
    }
    return out;
}

static SO6 gate_on_qubit(int q, const std::string& g) {
    if (g == "I") return SO6::identity();

    const SO6& H = (q == 0) ? H0() : H1();
    const SO6& S = (q == 0) ? S0() : S1();
    const SO6& T = (q == 0) ? T0() : T1();

    if (g == "H") return H;
    if (g == "S") return S;
    if (g == "S*") return transpose(S);
    if (g == "T") return T;
    if (g == "T*") return transpose(T);

    if (g == "Z") return S * S;
    if (g == "X") {
        SO6 Zm = S * S;
        return H * Zm * H;
    }
    if (g == "Y") {
        SO6 X = H * (S * S) * H;
        SO6 Sinv = transpose(S);
        return S * X * Sinv;
    }

    throw std::runtime_error("unsupported 1q token: " + g);
}

static SO6 controlled_pauli(int control_q, int target_q, const std::string& g) {
    if (g == "Z") return CZ();
    if (g != "X" && g != "Y") throw std::runtime_error("unsupported controlled gate: " + g);

    SO6 B = SO6::identity();
    if (g == "X") {
        B = gate_on_qubit(target_q, "H");
    } else {
        SO6 Ht = gate_on_qubit(target_q, "H");
        SO6 St = gate_on_qubit(target_q, "S");
        B = St * Ht;
    }
    SO6 Binv = transpose(B);
    return B * CZ() * Binv;
}

static SO6 stage_matrix(const std::string& a, const std::string& b) {
    auto is_control = [](const std::string& s) { return s.size() >= 3 && s.rfind("C(", 0) == 0; };

    if (!is_control(a) && !is_control(b)) {
        SO6 A0 = gate_on_qubit(0, a);
        SO6 A1 = gate_on_qubit(1, b);
        return A1 * A0;
    }

    if (is_control(a) && !is_control(b)) return controlled_pauli(0, 1, b);
    if (!is_control(a) && is_control(b)) return controlled_pauli(1, 0, a);

    throw std::runtime_error("unsupported stage with two controls");
}

static void trace_so6_step(std::ostream& os,
                           size_t step,
                           const std::string& a,
                           const std::string& b,
                           const SO6& stage,
                           const SO6& cur,
                           const SO6& next) {
    os << "[so6] step " << step << " gates: " << a << " " << b << "\n";
    os << "[so6] stage_matrix =\n";
    stage.print_raw(os);
    os << "\n";
    os << "[so6] current      =\n";
    cur.print_raw(os);
    os << "\n";
    os << "[so6] multiply stage * current\n";
    os << "[so6] next         =\n";
    next.print_raw(os);
    os << "\n";
}

static SO6 so6_of_rows(const std::vector<std::string>& row0,
                       const std::vector<std::string>& row1,
                       std::ostream* trace) {
    if (row0.size() != row1.size()) throw std::runtime_error("row lengths differ");
    SO6 cur = SO6::identity();
    for (size_t t = 0; t < row0.size(); ++t) {
        SO6 stage = stage_matrix(row0[t], row1[t]);
        SO6 next = stage * cur;
        if (trace) {
            trace_so6_step(*trace, t, row0[t], row1[t], stage, cur, next);
        }
        cur = next;
    }
    return cur;
}

static std::string join_tokens(const std::vector<std::string>& toks) {
    std::ostringstream ss;
    for (size_t i = 0; i < toks.size(); ++i) {
        if (i) ss << " ";
        ss << toks[i];
    }
    return ss.str();
}

static bool is_identity_equivalent(const SO6& target) {
    return target == SO6::identity();
}

static MITMMatchResult run_mitm_interactive(const amy::Circuit& circ,
                                            const SO6& target,
                                            int timeout_sec,
                                            bool print,
                                            bool pause) {
    if (print) {
        std::cout << "[interactive] row0 " << join_tokens(circ.row0) << "\n";
        std::cout << "[interactive] row1 " << join_tokens(circ.row1) << "\n";
        std::cout << "[interactive] target (mathematica):\n";
        target.print_mathematica(std::cout);
        std::cout << "\n";
    }

    MITM mitm(SO6::identity(), target);
    MITMMatchResult res;
    if (timeout_sec > 0) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_sec);
        res = generate_mitm_match(mitm, &deadline);
    } else {
        res = generate_mitm_match(mitm);
    }

    if (print) {
        std::cout << "[interactive] found=" << (res.found ? "true" : "false")
                  << " dl=" << res.dl
                  << " dr=" << res.dr
                  << " timed_out=" << (res.timed_out ? "true" : "false")
                  << "\n";
        if (res.found && res.dl >= 0 && res.dr >= 0) {
            std::cout << "[interactive] tcount=" << (res.dl + res.dr) << "\n";
        }
    }
    if (pause) {
        std::cout << "[interactive] press Enter to continue..." << std::flush;
        std::string line;
        std::getline(std::cin, line);
    }
    return res;
}

static void print_hardcoded_so6(std::ostream& os) {
    struct Entry {
        const char* label;
        const SO6& (*matrix)();
    };
    const Entry entries[] = {
        {"H on q0 (row0=H row1=I)", &H0},
        {"S on q0 (row0=S row1=I)", &S0},
        {"T on q0 (row0=T row1=I)", &T0},
        {"H on q1 (row0=I row1=H)", &H1},
        {"S on q1 (row0=I row1=S)", &S1},
        {"T on q1 (row0=I row1=T)", &T1},
        {"CZ (row0=C(2) row1=Z)", &CZ},
    };
    os << "[so6-hardcoded] begin\n";
    for (const auto& entry : entries) {
        os << "[so6-hardcoded] " << entry.label << "\n";
        entry.matrix().print_raw(os);
        os << "\n\n";
    }
    os << "[so6-hardcoded] end\n";
}

static int count_existing_labels(const fs::path& labels_path) {
    if (!fs::exists(labels_path)) return 0;
    std::ifstream in(labels_path);
    int count = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) ++count;
    }
    return count;
}

static void write_header_if_empty(const fs::path& path, const std::string& header) {
    if (fs::exists(path) && fs::file_size(path) > 0) return;
    std::ofstream out(path, std::ios::app);
    out << header;
}

} // namespace

struct Args {
    fs::path outdir{"benchset_amy_random"};
    uint64_t seed{1337};
    int count{30};
    int tcount{4};
    int search_depth{16};
    int timeout_sec{10};
    int threads{0};
    int progress_sec{0};
    double prob_t{0.2};
    amy::CircuitParams params{};
    bool verbose{false};
    bool interactive{false};
    bool print_so6{false};
    bool trace_so6{false};
    bool mitm_bf{false};
};

static void print_usage() {
    std::cout
        << "Usage: gen_amy_random_circuits [OUTDIR] [--seed=U64] [--count=N]\n"
        << "       [--tcount=N] [--prob-t=P]\n"
        << "       [--allow-cnot] [--allow-cz] [--trace-so6]\n"
        << "       [--search-depth=N] [--timeout-sec=N] [--threads=N]\n"
        << "       [--progress-sec=N] [--interactive] [--verbose] [--print-so6] [--mitm-bf]\n";
}

static Args parse_args(int argc, char** argv) {
    Args a;
    bool outdir_set = false;

    for (int i = 1; i < argc; ++i) {
        std::string s(argv[i]);
        auto val = [&](const char* key) -> const char* {
            size_t n = std::strlen(key);
            if (s.size() > n && s.compare(0, n, key) == 0 && s[n] == '=') {
                return s.c_str() + n + 1;
            }
            return nullptr;
        };

        if (s == "--help" || s == "-h") {
            print_usage();
            std::exit(0);
        }

        if (auto* v = val("--out")) a.outdir = v;
        else if (auto* v = val("--seed")) a.seed = std::strtoull(v, nullptr, 10);
        else if (auto* v = val("--count")) {
            a.count = std::max(1, std::atoi(v));
        }
        else if (auto* v = val("--tcount")) {
            a.tcount = std::max(0, std::atoi(v));
        }
        else if (auto* v = val("--prob-t")) a.prob_t = std::strtod(v, nullptr);
        else if (auto* v = val("--search-depth")) a.search_depth = std::max(1, std::atoi(v));
        else if (auto* v = val("--timeout-sec")) a.timeout_sec = std::max(0, std::atoi(v));
        else if (auto* v = val("--threads")) a.threads = std::max(0, std::atoi(v));
        else if (auto* v = val("--progress-sec")) a.progress_sec = std::max(0, std::atoi(v));
        else if (s == "--allow-cnot") a.params.allow_cnot = true;
        else if (s == "--allow-cz") a.params.allow_cnot = true; // alias for MITMS-friendly CNOTs
        else if (s == "--trace-so6") a.trace_so6 = true;
        else if (s == "--interactive") a.interactive = true;
        else if (s == "--verbose") a.verbose = true;
        else if (s == "--print-so6") a.print_so6 = true;
        else if (s == "--mitm-bf") a.mitm_bf = true;
        else if (!outdir_set && !s.empty() && s[0] != '-') {
            a.outdir = s;
            outdir_set = true;
        } else {
            std::cerr << "Unknown argument: " << s << "\n";
            print_usage();
            std::exit(2);
        }
    }

    return a;
}

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);
    args.params.prob_t = args.prob_t;
    if (args.interactive) {
        args.verbose = true;
    }
    if (args.print_so6) {
        print_hardcoded_so6(std::cout);
    }

    suppress_indicators = true;
    stored_depth_max = static_cast<uint8_t>(args.search_depth);
    target_T_count = static_cast<uint8_t>(args.search_depth + 1);
    mitm_bf_extension = args.mitm_bf;
    if (args.threads > 0) {
        THREADS = static_cast<uint8_t>(args.threads);
    } else {
        THREADS = static_cast<uint8_t>(std::max(1u, std::thread::hardware_concurrency()));
    }

    fs::create_directories(args.outdir);

    std::mt19937_64 rng(args.seed);

    struct BucketWriter {
        bool inited = false;
        int next_idx = 0;
        int run_count = 0;
        fs::path dir;
        std::ofstream searches;
        std::ofstream labels;
        std::ofstream meta;
    };

    std::vector<BucketWriter> bucket_writers;
    int attempts = 0;
    int accepted = 0;
    int timeouts = 0;
    int failures = 0;
    std::string last_event = "init";

    const int progress_sec = args.progress_sec;
    auto last_progress = std::chrono::steady_clock::now();
    auto log_progress = [&]() {
        if (progress_sec <= 0) return;
        auto now = std::chrono::steady_clock::now();
        if (now - last_progress < std::chrono::seconds(progress_sec)) return;
        last_progress = now;

        std::ostringstream dist;
        bool first = true;
        for (size_t i = 0; i < bucket_writers.size(); ++i) {
            if (!bucket_writers[i].inited) continue;
            if (!first) dist << " ";
            first = false;
            dist << i << ":" << bucket_writers[i].run_count;
        }
        if (first) dist << "-";

        if (args.verbose) {
            std::cout << "[bucket] attempts=" << attempts
                      << " accepted=" << accepted << "/" << args.count
                      << " timeouts=" << timeouts
                      << " failures=" << failures
                      << " last=" << last_event
                      << " dist={" << dist.str() << "}\n";
        }
    };

    std::unique_ptr<tbb::global_control> tbb_control;
    tbb_control = std::make_unique<tbb::global_control>(
        tbb::global_control::max_allowed_parallelism,
        static_cast<std::size_t>(std::max<uint8_t>(1, THREADS)));

    std::signal(SIGINT, handle_stop_signal);
    std::signal(SIGTERM, handle_stop_signal);

    while (accepted < args.count) {
        ++attempts;
        args.params.tcount = args.tcount;
        amy::Circuit circ = amy::random_circuit(rng, args.params);

        if (args.trace_so6) {
            std::cout << "[so6] row0 " << join_tokens(circ.row0) << "\n";
            std::cout << "[so6] row1 " << join_tokens(circ.row1) << "\n";
        }
        SO6 target = so6_of_rows(circ.row0, circ.row1, args.trace_so6 ? &std::cout : nullptr);
        if (is_identity_equivalent(target)) {
            last_event = "identity";
            log_progress();
            continue;
        }
        target.last_T = 15;
        MITMMatchResult res = run_mitm_interactive(
            circ, target, args.timeout_sec, args.verbose, args.interactive);
        if (!res.found || res.dl < 0 || res.dr < 0) {
            if (res.timed_out) {
                ++timeouts;
                last_event = "timeout";
                if (args.verbose) {
                    std::cout << "[bucket] timeout target matrix (attempt " << attempts
                              << ", tdepth " << circ.tdepth << ", mathematica):\n";
                    target.print_mathematica(std::cout);
                    std::cout << "\n";
                    std::ostringstream timeout_label;
                    timeout_label << "timeout_attempt_" << std::setw(4) << std::setfill('0') << attempts
                                  << "_td" << circ.tdepth;
                    std::cout << "[bucket] timeout circuit (searches format):\n";
                    amy::write_searches_block(std::cout, timeout_label.str(), circ);
                }
            } else {
                ++failures;
                last_event = "fail";
            }
            log_progress();
            continue;
        }

        const int tcount = res.dl + res.dr;
        if (tcount < 0) {
            last_event = "bad_tcount";
            log_progress();
            continue;
        }
        if (static_cast<size_t>(tcount) >= bucket_writers.size()) {
            bucket_writers.resize(static_cast<size_t>(tcount) + 1);
        }
        BucketWriter& writer = bucket_writers[static_cast<size_t>(tcount)];
        if (!writer.inited) {
            writer.dir = args.outdir / ("tcount_" + std::to_string(tcount));
            fs::create_directories(writer.dir / "targets");
            fs::path searches_path = writer.dir / "searches";
            fs::path labels_path = writer.dir / "labels.txt";
            fs::path meta_path = writer.dir / "meta.csv";
            write_header_if_empty(meta_path, "label,tdepth,tcount,depth,dl,dr\n");
            writer.searches.open(searches_path, std::ios::app);
            writer.labels.open(labels_path, std::ios::app);
            writer.meta.open(meta_path, std::ios::app);
            writer.next_idx = count_existing_labels(labels_path);
            writer.inited = true;
        }
        int& idx = writer.next_idx;
        std::ostringstream lab;
        lab << "amy_tc" << std::setw(2) << std::setfill('0') << tcount
            << "_" << std::setw(4) << std::setfill('0') << idx;
        const std::string label = lab.str();

        amy::write_searches_block(writer.searches, label, circ);
        writer.labels << label << "\n";
        writer.meta << label << "," << circ.tdepth << ","
                    << tcount << "," << circ.depth() << ","
                    << res.dl << "," << res.dr << "\n";
        std::ofstream tf(writer.dir / "targets" / (label + ".mat"));
        target.print_mathematica(tf);
        tf << "\n";
        ++writer.run_count;
        ++accepted;
        last_event = "accept tcount=" + std::to_string(tcount);
        log_progress();
        ++idx;

        if (stop_requested()) break;
    }

    fs::path summary_path = args.outdir / "summary.csv";
    write_header_if_empty(summary_path, "tcount,count\n");
    std::ofstream summary(summary_path, std::ios::app);

    for (size_t tcount = 0; tcount < bucket_writers.size(); ++tcount) {
        const BucketWriter& writer = bucket_writers[tcount];
        if (!writer.inited || writer.run_count == 0) continue;
        summary << tcount << "," << writer.run_count << "\n";
    }

    if (args.verbose) {
        std::cout << "Generated " << accepted << " circuits (attempted " << attempts << ") in "
                  << args.outdir << "\n";
        if (timeouts > 0 || failures > 0) {
            std::cout << "Skipped: " << timeouts << " timeouts, " << failures << " failures\n";
        }
    }
    return 0;
}
