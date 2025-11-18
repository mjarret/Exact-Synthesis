/**
 * Minimal MITM matcher: alternating expansions until the first intersection.
 *
 * Assumptions:
 *  - The target SO6 to search for is provided by the caller.
 *  - Global depth parameters (stored_depth_max / target_T_count / THREADS) are configured externally.
 *
 * TODO: Replace the placeholder target in main() with your actual input source (CLI, file, etc.).
 * TODO: Replace the placeholder result handling in main() with logging/verification you care about.
 */

#include <random>
#include <iostream>
#include <bitset>
#include <array>
#include <algorithm>
#include <cstring> // for std::strlen

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "ds/LUT.hpp"
#include "ds/MITM.hpp"
#include "algo/Generate.hpp"
#include "ds/Lehmer6.hpp"

namespace {

// Minimal layer lookup: returns smallest layer index containing x, or -1 if absent.
int layer_of(LUT& lut, const SO6& x) {
    int layer = 0;
    for (auto it = lut.layers_begin(); it != lut.layers_end(); ++it, ++layer) {
        if (it->find(x) != it->end()) return layer;
    }
    return -1;
}

// Find the stored SO6 equal to `key` inside a LUT. Returns pointer or nullptr.
// Optionally returns the layer it was found in.
const SO6* lookup_in(LUT& lut, const SO6& key, int* layer_out = nullptr) {
    int layer = 0;
    for (auto it = lut.layers_begin(); it != lut.layers_end(); ++it, ++layer) {
        auto f = it->find(key);
        if (f != it->end()) {
            if (layer_out) *layer_out = layer;
            return &*f;
        }
    }
    if (layer_out) *layer_out = -1;
    return nullptr;
}

std::array<uint8_t,6> invert_perm(const std::array<uint8_t,6>& p) {
    std::array<uint8_t,6> inv{};
    for (size_t i=0;i<6;++i) inv[p[i]] = static_cast<uint8_t>(i);
    return inv;
}

std::array<uint8_t,6> compose_perm(const std::array<uint8_t,6>& a, const std::array<uint8_t,6>& b) {
    std::array<uint8_t,6> out{};
    for (size_t i=0;i<6;++i) out[i] = a[b[i]];
    return out;
}

std::array<uint8_t,6> decode_perm(const Lehmer6& lh) {
    return Lehmer6::decode_ref(lh.bits());
}

SO6 apply_mapping(const SO6& src, const std::array<uint8_t,6>& row_map, const std::array<uint8_t,6>& col_map, uint8_t sign_mask) {
    SO6 out;
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            Z2 v = src.get_element(row_map[static_cast<size_t>(r)], col_map[static_cast<size_t>(c)]);
            if ((sign_mask >> r) & 1u) v = -v;
            out.set_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c), v);
        }
    }
    return out;
}

SO6 unapply_perms(const SO6& canon) {
    const auto& rp = decode_perm(canon.row_perm_lh_);
    const auto& cp = decode_perm(canon.col_perm_lh_);
    std::array<uint8_t,6> inv_r = invert_perm(rp);
    std::array<uint8_t,6> inv_c = invert_perm(cp);
    SO6 raw;
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            uint8_t src_r = rp[static_cast<size_t>(r)];
            uint8_t src_c = cp[static_cast<size_t>(c)];
            auto v = canon.get_element(src_r, src_c);
            // Undo sign choice applied during canonicalization.
            if ((canon.sign_convention >> src_r) & 1u) v = -v;
            raw.set_element(inv_r[static_cast<size_t>(r)], inv_c[static_cast<size_t>(c)], v);
        }
    }
    return raw;
}

void print_matrix(const SO6& m, const char* label) {
    std::cout << label << "\n";
    for (int r = 0; r < 6; ++r) {
        std::cout << "  [";
        for (int c = 0; c < 6; ++c) {
            auto v = m.get_element(r,c);
            std::cout << int(v.int_c);
            if (c != 5) std::cout << " ";
        }
        std::cout << "]\n";
    }
}

// Derive per-row sign flips to make apply_mapping(left_raw, ...) == right_raw.
uint8_t derive_sign_mask(const SO6& left_raw, const SO6& right_raw,
                         const std::array<uint8_t,6>& row_map,
                         const std::array<uint8_t,6>& col_map)
{
    uint8_t s = 0;
    for (int r = 0; r < 6; ++r) {
        bool all_eq  = true;
        bool all_neg = true;
        for (int c = 0; c < 6; ++c) {
            auto lv = left_raw.get_element(row_map[static_cast<size_t>(r)],
                                           col_map[static_cast<size_t>(c)]);
            auto rv = right_raw.get_element(r, c);
            if (lv   != rv) all_eq  = false;
            if (-lv  != rv) all_neg = false;
        }
        if (all_neg) s |= static_cast<uint8_t>(1u << r);
    }
    return s;
}

struct MatchResult {
    bool found{false};
    SO6 meet{};
    int side{-1}; // 0 = left (identity-root), 1 = right (target-root)
    int dl{-1};   // depth from identity to meet
    int dr{-1};   // depth from target to meet
    std::array<uint8_t,6> row_map{};
    std::array<uint8_t,6> col_map{};
    uint8_t sign_mask{0};
    bool mapping_ok{false};
    SO6 left_raw{};
    SO6 right_raw{};
};

// Core alternating MITM loop with progress bars: expand left then right until the first intersection or depth limit.
std::pair<MITM,SO6> run_mitm_with_progress(const SO6& target) {
    MITM mitm(SO6::identity(), target);
    auto result = generate_mitm_until_match(mitm);
    if (!result) return {};
    return {mitm, *result};
}

} // namespace

struct Args {
    int depth{4};          // number of random T steps to build the second root
    int search_depth{10};  // BFS depth limit for each side
    uint64_t seed{0};
    int threads{0};
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s(argv[i]);
        auto val = [&](const char* key) -> const char* {
            size_t n = std::strlen(key);
            if (s.size() > n && s.compare(0, n, key) == 0 && s[n] == '=') return s.c_str() + n + 1;
            return nullptr;
        };
        if (s == "--help" || s == "-h") {
        std::cout << "Usage: mitm_match_tester [--depth=N] [--search-depth=N] [--seed=U64] [--threads=N]\n";
        std::exit(0);
    }
    if (auto* v = val("--depth"))   a.depth   = std::max(1, std::atoi(v));
    else if (auto* v = val("--search-depth")) a.search_depth = std::max(1, std::atoi(v));
    else if (auto* v = val("--seed")) a.seed  = std::strtoull(v, nullptr, 10);
    else if (auto* v = val("--threads")) a.threads = std::max(0, std::atoi(v));
}
return a;
}

SO6 random_target(std::mt19937_64& rng, int steps) {
    std::uniform_int_distribution<int> d(0, 14);
    SO6 cur = SO6::identity();
    for (int i = 0; i < steps; ++i) {
        cur = T_OperatorRuntime(static_cast<uint8_t>(d(rng))) * cur;
    }
    return cur;
}

SO6 random_perm_sign(const SO6& base, std::mt19937_64& rng) {
    std::array<uint8_t,6> row{0,1,2,3,4,5};
    std::array<uint8_t,6> col{0,1,2,3,4,5};
    std::shuffle(row.begin(), row.end(), rng);
    std::shuffle(col.begin(), col.end(), rng);
    uint8_t sign_mask = static_cast<uint8_t>(rng() & 0x3Fu);

    // Apply permutation/sign directly to the raw data
    SO6 out;
    for (int r = 0; r < 6; ++r) {
        uint8_t src_r = row[static_cast<size_t>(r)];
        bool neg = ((sign_mask >> r) & 1u) != 0;
        for (int c = 0; c < 6; ++c) {
            uint8_t src_c = col[static_cast<size_t>(c)];
            auto v = base.get_element(src_r, src_c);
            if (neg) v = -v;
            out.set_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c), v);
        }
    }
    // Reset permutation/sign metadata to identity for this new raw layout
    out.last_T = base.last_T;
    out.sign_convention = 0;
    out.row_perm_lh_ = Lehmer6(); // identity
    out.col_perm_lh_ = Lehmer6(); // identity
    out.recompute_hash();
    return out;
}

SO6 combine_path(SO6 left, SO6 right)
{
    // Capture original row permutations to correctly compose sign masks.
    const auto& L_old = Lehmer6::decode_ref(left.row_perm_lh_.bits());   // pos -> srcRow
    const auto& R_arr = Lehmer6::decode_ref(right.row_perm_lh_.bits());  // pos -> srcRow

    // Build srcRow -> position maps for left (old) and right frames.
    uint8_t posL_old[6]{};
    uint8_t posR[6]{};
    for (uint8_t i = 0; i < 6; ++i) {
        posL_old[L_old[i]] = i;
        posR[R_arr[i]] = i;
    }


    std::cout << "\n == RAW MATRICES BEFORE COMBINE ==\n\n";
    left.print_raw(std::cout);
    std::cout << "\n AND \n\n";
    right.print_raw(std::cout);

    std::cout << "\n == PERM COMPOSITION DETAILS ==\n\n";
    std::cout << " Left row perm (old): " << left.row_perm_lh_ << "\n";
    std::cout << " Right row perm:      " << right.row_perm_lh_ << "\n";
    std::cout << " Left col perm (old): " << left.col_perm_lh_ << "\n";
    std::cout << " Right col perm:      " << right.col_perm_lh_ << "\n";
    std::cout << " Right inverse row perm:  " << right.row_perm_lh_.inverse() << "\n";
    std::cout << " Right inverse col perm:  " << right.col_perm_lh_.inverse() << "\n";

    // Compose permutations in Lehmer space (apply right^{-1} to left).
    left.row_perm_lh_ =  left.row_perm_lh_ * right.row_perm_lh_.inverse();
    left.col_perm_lh_ =  left.col_perm_lh_ * right.col_perm_lh_.inverse();

    std::cout << " Composed row perm:   " << left.row_perm_lh_ << "\n";
    std::cout << " Composed col perm:   " << left.col_perm_lh_ << "\n";
    std::cout << "\n";
    std::cout << " After perm compose, before sign fix:\n";
    left.print_with_perms(std::cout);

    std::cin.get();

    // New output frame: srcRow -> position mapping under the composed row permutation.
    const auto& L_out = Lehmer6::decode_ref(left.row_perm_lh_.bits()); // pos -> srcRow
    uint8_t posOut[6]{};                                               // srcRow -> pos
    for (uint8_t i = 0; i < 6; ++i) posOut[L_out[i]] = i;

    // Map both sign masks into the new frame (by position) and XOR.
    uint8_t sL = left.sign_convention;
    uint8_t sR = right.sign_convention;
    uint8_t sL_mapped = 0;
    uint8_t sR_mapped = 0;
    for (uint8_t src = 0; src < 6; ++src) {
        const uint8_t bitL = static_cast<uint8_t>((sL >> posL_old[src]) & 1u);
        const uint8_t bitR = static_cast<uint8_t>((sR >> posR[src]) & 1u);
        if (bitL) sL_mapped |= static_cast<uint8_t>(1u << posOut[src]);
        if (bitR) sR_mapped |= static_cast<uint8_t>(1u << posOut[src]);
    }
    left.sign_convention = static_cast<uint8_t>(sL_mapped ^ sR_mapped);
    std::cout << " After sign combine:      " << std::bitset<6>(left.sign_convention) << "\n";
    left.print_with_perms(std::cout);
    std::cout << "\n";
    std::cin.get();
    return left;
}

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);

    suppress_indicators = false; // show MITM progress
    stored_depth_max = static_cast<uint8_t>(args.search_depth);
    target_T_count = static_cast<uint8_t>(args.search_depth + 1);
    if (args.threads > 0) THREADS = static_cast<uint8_t>(args.threads);

    std::mt19937_64 rng(args.seed ? args.seed : std::random_device{}());

    // Pick a random target by a short random walk of length = depth, then random perm/sign
    SO6 target = random_target(rng, args.depth);
    target = random_perm_sign(target, rng);

    auto [mitm, result] = run_mitm_with_progress(target);
    auto left = mitm.left();
    auto right = mitm.right();

    SO6 left_meet = *left.find(result);
    SO6 right_meet = *right.find(result);

    left_meet.print_with_perms(std::cout);
    std::cout << "\n == MEETS WITH ==\n\n";
    right_meet.print_with_perms(std::cout);
    std::cout << "\n";

    std::cout << "\n == RAW MATRICES ==\n\n";
    left_meet.print_raw(std::cout);
    std::cout << "\n == AND ==\n\n";
    right_meet.print_raw(std::cout);

    std::cout << "\n == COMBINED PATH ==\n\n";

    SO6 combined = combine_path(left_meet, right_meet);
    combined.print_raw(std::cout);
    std::cout << "\n\n == COMBINED AFTER RIGHT T-CHAIN ==\n\n";

    // Walk the path from the right root to right_meet (using stored last_T),
    // but apply the operators in reverse to unwind back to the right root.
    if (auto path = right.path_to(right_meet)) {
        std::reverse(path->begin(), path->end());
        SO6 unwound = combined.materialize_canonical();
        for (auto it = path->rbegin(); it != path->rend(); ++it) {
            unwound = T_OperatorRuntime(*it) * unwound;
            std::cout << " after T" << static_cast<int>(*it) << ":\n";
            unwound.print_raw(std::cout);
            std::cout << "\n";
            std::cout << " right after T" << static_cast<int>(*it) << ":\n";
            SO6 right_step = T_OperatorRuntime(*it) * right_meet;
            right_step.print_raw(std::cout);
            right_meet = right_step;
            std::cout << "\n";
            std::cin.get();
        }
        std::cout << "\n";
        combined = unwound;
    } else {
        std::cout << "[error] could not recover path on right side\n";
    }
    target = target.materialize_canonical();
    std::cout << "\n == VERIFY EQUALITY TO TARGET UP TO COLUMN SIGNS ==\n\n";
    target.print_raw(std::cout);
    std::cout << "\n == VS COMBINED ==\n\n";
    combined.print_raw(std::cout);
    std::cout << "\n";
    target == combined ? std::cout << "\nSUCCESS: combined matrix matches target!\n"
                             : std::cout << "\nFAILURE: combined matrix does NOT match target!\n";
    std::cout << "\n";
    return 0;
}
