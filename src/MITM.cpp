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

// Build two lookup tables in alternating layers until either depth limit hits
// or an intersection is found. Returns the meeting SO6 if found, nullopt otherwise.
std::optional<SO6> generate_mitm_until_match(MITM& mitm) {
    LUT& left = mitm.left();
    LUT& right = mitm.right();
    SO6 meet{};

    for (int depth = 0; depth < 2*stored_depth_max; ++depth) {
        // Expand only the smaller frontier this iteration
        bool expand_left = left.current().size() <= right.current().size();

        bool hit = false;
        LUT& active = expand_left ? left : right;
        LUT& passive = expand_left ? right : left;
        const char* label = expand_left ? "L" : "R";

        auto pred = [&](const SO6& s){ bool r = (passive.back().find(s) != passive.back().end()); if (r) hit = true; return r; };
        std::unique_ptr<indicators::ProgressTracker> bar =
            std::make_unique<indicators::ProgressTracker>(active.size() - 1, active.current().size() * 15, active.current().size() * 15, label);
        algo::get_next_T_count(active, bar.get(), pred, &meet);
        active.finalize_current_set(bar.get());

        if (hit) return meet;
    }
    return std::nullopt;
}

// Attempt to find the row/col/sign mapping to transform lhs into rhs exactly.
// Returns true and fills mappings if successful.
bool reconcile_matrices(const SO6& lhs, const SO6& rhs,
                        std::array<uint8_t,6>& row_map,
                        std::array<uint8_t,6>& col_map,
                        uint8_t& sign_mask_out) {
    auto decode = [](const Lehmer6& lh){
        return Lehmer6::decode_ref(lh.bits());
    };
    const auto& rpL = decode(lhs.row_perm_lh_);
    const auto& rpR = decode(rhs.row_perm_lh_);
    const auto& cpL = decode(lhs.col_perm_lh_);
    const auto& cpR = decode(rhs.col_perm_lh_);

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
        bool lhs_neg = ((lhs.sign_convention >> lhs_row) & 1u) != 0;
        bool rhs_neg = ((rhs.sign_convention >> i) & 1u) != 0;
        if (lhs_neg != rhs_neg) sign_mask_out |= static_cast<uint8_t>(1u << i);
    }

    auto matches = [&](const std::array<uint8_t,6>& rm, const std::array<uint8_t,6>& cm, uint8_t sm)->bool{
        for (int r = 0; r < 6; ++r) {
            for (int c = 0; c < 6; ++c) {
                Z2 val = lhs.get_element(rm[static_cast<size_t>(r)], cm[static_cast<size_t>(c)]);
                if ( (sm >> r) & 1u ) val = -val;
                if (val != rhs.get_element(r,c)) return false;
            }
        }
        return true;
    };

    if (matches(row_map, col_map, sign_mask_out)) return true;

    // Fallback: brute-force row/col permutations and sign mask
    for (uint16_t rcode = 0; rcode < 720; ++rcode) {
        const auto& rm = Lehmer6::decode_ref(rcode);
        for (uint16_t ccode = 0; ccode < 720; ++ccode) {
            const auto& cm = Lehmer6::decode_ref(ccode);
            for (uint8_t sm = 0; sm < 64; ++sm) {
                std::array<uint8_t,6> rm_arr = rm;
                std::array<uint8_t,6> cm_arr = cm;
                if (matches(rm_arr, cm_arr, sm)) {
                    row_map = rm_arr;
                    col_map = cm_arr;
                    sign_mask_out = sm;
                    return true;
                }
            }
        }
    }
    return false;
}
