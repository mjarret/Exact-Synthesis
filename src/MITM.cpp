/**
 * @file MITM.cpp
 * @brief Minimal MITM generator utilities.
 *
 * Provides a helper to expand two LUTs in alternating layers until an
 * intersection is found, mirroring the style of algo::create_lookup_table but
 * for dual roots.
 */

#include "ds/MITM.hpp"
#include "algo/Generate.hpp"
#include "config/Globals.hpp"
#include "util/progress_tracker.hpp"
#include "ds/Lehmer6.hpp"
#include "so6/T_Operator.hpp"
#include <atomic>
#include <cstddef>
#include <limits>
#include <thread>
#include <tbb/global_control.h>
#include <tbb/parallel_for_each.h>

namespace {
std::size_t mitm_thread_limit() {
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 1;
    unsigned int requested = THREADS;
    if (requested == 0) requested = hw;
    if (requested > hw) requested = hw;
    return static_cast<std::size_t>(requested);
}

constexpr int kBruteforceTimeMultiplier = 10;

struct BruteforceHit {
    SO6 meet{};
    int extra_depth{0};
};

std::optional<BruteforceHit> brute_force_extend_until_match(
    const LUT& active,
    const LUT& passive,
    const std::chrono::steady_clock::time_point& deadline) {
    const auto& leaves = active.current();
    if (leaves.empty()) return std::nullopt;

    auto time_exceeded = [&]() {
        return std::chrono::steady_clock::now() >= deadline;
    };

    for (int depth = 1; !time_exceeded(); ++depth) {
        std::size_t count = 1;
        bool overflow = false;
        for (int i = 0; i < depth; ++i) {
            if (count > std::numeric_limits<std::size_t>::max() / 14u) {
                overflow = true;
                break;
            }
            count *= 14u;
        }
        if (overflow) break;

        std::atomic<bool> stop{false};
        std::atomic<bool> found{false};
        std::atomic_flag winner_claimed = ATOMIC_FLAG_INIT;
        SO6 winner{};

        tbb::parallel_for_each(leaves.begin(), leaves.end(), [&](const SO6& leaf) {
            if (stop.load(std::memory_order_relaxed)) return;
            if (time_exceeded()) {
                stop.store(true, std::memory_order_relaxed);
                return;
            }
            const uint8_t last_T = leaf.last_T;
            for (std::size_t code = 0; code < count && !stop.load(std::memory_order_relaxed); ++code) {
                if ((code & 0xFFu) == 0u && time_exceeded()) {
                    stop.store(true, std::memory_order_relaxed);
                    break;
                }
                SO6 cur = leaf;
                std::size_t x = code;
                uint8_t forbid = last_T;
                for (int pos = 0; pos < depth; ++pos) {
                    uint8_t d = static_cast<uint8_t>(x % 14u);
                    x /= 14u;
                    uint8_t t = static_cast<uint8_t>(d + (d >= forbid ? 1u : 0u));
                    cur = T_OperatorRuntime(t) * cur;
                    forbid = t;
                }
                if (passive.find(cur) != passive.end()) {
                    stop.store(true, std::memory_order_relaxed);
                    found.store(true, std::memory_order_relaxed);
                    if (!winner_claimed.test_and_set(std::memory_order_acq_rel)) {
                        winner = cur;
                    }
                    break;
                }
            }
        });

        if (found.load(std::memory_order_relaxed)) {
            return BruteforceHit{winner, depth};
        }
    }

    return std::nullopt;
}
} // namespace

// Build two lookup tables in alternating layers until either depth limit hits
// or an intersection is found. Returns the meeting SO6 if found, nullopt otherwise.
std::optional<SO6> generate_mitm_until_match(MITM& mitm) {
    return generate_mitm_until_match(mitm, nullptr);
}

std::optional<SO6> generate_mitm_until_match(
    MITM& mitm,
    const std::chrono::steady_clock::time_point* deadline) {
    tbb::global_control gc(tbb::global_control::max_allowed_parallelism,
                           mitm_thread_limit());
    mitm.clear_bruteforce_info();
    LUT& left = mitm.left();
    LUT& right = mitm.right();
    SO6 meet{};

    bool hit = false;
    for (int depth = 0; depth < 2*stored_depth_max; ++depth) {
        if (deadline && std::chrono::steady_clock::now() >= *deadline) {
            return std::nullopt;
        }
        // Expand only the smaller frontier this iteration
        bool expand_left = left.current().size() <= right.current().size();

        LUT& active = expand_left ? left : right;
        LUT& passive = expand_left ? right : left;
        const char* label = expand_left ? "L" : "R";

        auto pred = [&](const SO6& s){ bool r = (passive.back().find(s) != passive.back().end()); if (r) hit = true; return r; };
        std::unique_ptr<indicators::ProgressTracker> bar =
            std::make_unique<indicators::ProgressTracker>(active.size() - 1, active.current().size() * 15, active.current().size() * 15, label);
        const auto layer_start = std::chrono::steady_clock::now();
        algo::get_next_T_count(active, bar.get(), pred, &meet, deadline);
        active.finalize_current_set(bar.get());
        const auto layer_end = std::chrono::steady_clock::now();

        if (deadline && std::chrono::steady_clock::now() >= *deadline) {
            return std::nullopt;
        }
        if (hit) return meet;

        if (mitm_bf_extension && layer_end > layer_start) {
            auto bf_deadline = layer_end + (layer_end - layer_start) * kBruteforceTimeMultiplier;
            if (deadline && bf_deadline > *deadline) bf_deadline = *deadline;
            if (bf_deadline > layer_end) {
                bool bf_left = left.current().size() <= right.current().size();
                const LUT& bf_active = bf_left ? left : right;
                const LUT& bf_passive = bf_left ? right : left;
                auto bf_hit = brute_force_extend_until_match(bf_active, bf_passive, bf_deadline);
                if (bf_hit) {
                    mitm.set_bruteforce_info(bf_left, bf_hit->extra_depth);
                    return bf_hit->meet;
                }
            }
        }
    }
    return std::nullopt;
}

MITMMatchResult generate_mitm_match(MITM& mitm) {
    return generate_mitm_match(mitm, nullptr);
}

MITMMatchResult generate_mitm_match(
    MITM& mitm,
    const std::chrono::steady_clock::time_point* deadline) {
    MITMMatchResult result;

    auto meet_opt = generate_mitm_until_match(mitm, deadline);
    if (!meet_opt) {
        if (deadline && std::chrono::steady_clock::now() >= *deadline) {
            result.timed_out = true;
        }
        return result; // found stays false
    }

    result.found = true;
    result.meet = *meet_opt;

    // Recover paths from each root to the meeting element.
    auto left_path_opt  = mitm.left().path_to(result.meet);
    auto right_path_opt = mitm.right().path_to(result.meet);
    if (left_path_opt) {
        result.left_path  = std::move(*left_path_opt);
        result.dl = static_cast<int>(result.left_path.size());
    }
    if (right_path_opt) {
        result.right_path = std::move(*right_path_opt);
        result.dr = static_cast<int>(result.right_path.size());
    }

    const auto& bf_info = mitm.bruteforce_info();
    if (bf_info.used) {
        if (bf_info.from_left && result.dl < 0) {
            result.dl = mitm.depth_left() + bf_info.extra_depth;
        }
        if (!bf_info.from_left && result.dr < 0) {
            result.dr = mitm.depth_right() + bf_info.extra_depth;
        }
    }

    // Find canonical representatives of the meet in each LUT and derive a
    // row/col/sign mapping between them.
    auto left_it  = mitm.left().find(result.meet);
    auto right_it = mitm.right().find(result.meet);
    if (left_it != mitm.left().end() && right_it != mitm.right().end()) {
        const SO6& lhs = *left_it;
        const SO6& rhs = *right_it;
        std::array<uint8_t,6> row_map{};
        std::array<uint8_t,6> col_map{};
        uint8_t sign_mask = 0;
        uint8_t col_sign_mask = 0;
        if (reconcile_matrices(lhs, rhs, row_map, col_map, sign_mask, col_sign_mask)) {
            result.row_map = row_map;
            result.col_map = col_map;
            result.sign_mask = sign_mask;
            result.col_sign_mask = col_sign_mask;
            result.mapping_ok = true;
        }
    }

    return result;
}

// Attempt to find the row/col/sign mapping to transform lhs into rhs exactly.
// Returns true and fills mappings if successful.
namespace {
bool matches_with_col_sign(const SO6& lhs, const SO6& rhs,
                           const std::array<uint8_t,6>& row_map,
                           const std::array<uint8_t,6>& col_map,
                           uint8_t row_sign_mask,
                           uint8_t& col_sign_mask_out) {
    uint8_t col_mask = 0;
    for (int c = 0; c < 6; ++c) {
        bool have_sign = false;
        bool col_neg = false;
        for (int r = 0; r < 6; ++r) {
            DyadicSqrt2 val = lhs.get_element(row_map[static_cast<size_t>(r)],
                                              col_map[static_cast<size_t>(c)]);
            if ((row_sign_mask >> r) & 1u) val = -val;
            const DyadicSqrt2 rhs_val = rhs.get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c));
            if (val.int_c == 0 && rhs_val.int_c == 0) {
                continue;
            }
            if (val.int_c == 0 || rhs_val.int_c == 0) {
                return false;
            }
            bool need_neg = false;
            if (val == rhs_val) {
                need_neg = false;
            } else if (-val == rhs_val) {
                need_neg = true;
            } else {
                return false;
            }
            if (!have_sign) {
                col_neg = need_neg;
                have_sign = true;
            } else if (col_neg != need_neg) {
                return false;
            }
        }
        if (col_neg) col_mask |= static_cast<uint8_t>(1u << c);
    }
    col_sign_mask_out = col_mask;
    return true;
}
} // namespace

bool reconcile_matrices(const SO6& lhs, const SO6& rhs,
                        std::array<uint8_t,6>& row_map,
                        std::array<uint8_t,6>& col_map,
                        uint8_t& sign_mask_out,
                        uint8_t& col_sign_mask_out) {
    auto decode = [](const Lehmer6& lh){
        return Lehmer6::decode_ref(lh.bits());
    };
    const auto& rpL = decode(lhs.row_perm_lh());
    const auto& rpR = decode(rhs.row_perm_lh());
    const auto& cpL = decode(lhs.col_perm_lh());
    const auto& cpR = decode(rhs.col_perm_lh());

    // inv perm for rhs
    auto invert = [](const std::array<uint8_t,6>& p){
        std::array<uint8_t,6> inv{};
        for (size_t i=0;i<6;++i) inv[p[i]] = static_cast<uint8_t>(i);
        return inv;
    };
    auto invRpR = invert(rpR);
    auto invCpR = invert(cpR);

    for (size_t i=0;i<6;++i) {
        row_map[i] = rpL[invRpR[i]];
        col_map[i] = cpL[invCpR[i]];
    }

    // compute sign mask aligned to RHS rows
    sign_mask_out = 0;
    for (size_t i=0;i<6;++i) {
        uint8_t lhs_row = row_map[i];
        bool lhs_neg = ((lhs.sign_mask() >> lhs_row) & 1u) != 0;
        bool rhs_neg = ((rhs.sign_mask() >> i) & 1u) != 0;
        if (lhs_neg != rhs_neg) sign_mask_out |= static_cast<uint8_t>(1u << i);
    }

    if (matches_with_col_sign(lhs, rhs, row_map, col_map, sign_mask_out, col_sign_mask_out)) {
        return true;
    }

    // Fallback: brute-force row/col permutations and sign mask
    for (uint16_t rcode = 0; rcode < 720; ++rcode) {
        const auto& rm = Lehmer6::decode_ref(rcode);
        for (uint16_t ccode = 0; ccode < 720; ++ccode) {
            const auto& cm = Lehmer6::decode_ref(ccode);
            for (uint8_t sm = 0; sm < 64; ++sm) {
                std::array<uint8_t,6> rm_arr = rm;
                std::array<uint8_t,6> cm_arr = cm;
                uint8_t col_mask = 0;
                if (matches_with_col_sign(lhs, rhs, rm_arr, cm_arr, sm, col_mask)) {
                    row_map = rm_arr;
                    col_map = cm_arr;
                    sign_mask_out = sm;
                    col_sign_mask_out = col_mask;
                    return true;
                }
            }
        }
    }
    return false;
}
