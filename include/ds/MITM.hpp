/**
 * @file MITM.hpp
 * @brief Dual-root lookup helper for meet-in-the-middle scenarios.
 *
 * This is a thin convenience wrapper that owns two LUTs (left/right) rooted at
 * different seeds and exposes easy accessors for simultaneous expansion and
 * iteration. It mirrors the shape of LUT but keeps the two sides together.
 *
 * NOTE: No changes are made to LUT itself; this is purely an ergonomic layer.
 */
#ifndef MITM_HPP
#define MITM_HPP

#include <array>
#include <vector>

#include "ds/LUT.hpp"
#include "util/progress_tracker.hpp"

class MITM {
public:
    MITM(const SO6& left_root = SO6::identity(), const SO6& right_root = SO6::identity())
        : left_(left_root), right_(right_root) {}

    // Accessors
    LUT& left() { return left_; }
    LUT& right() { return right_; }
    const LUT& left() const { return left_; }
    const LUT& right() const { return right_; }

    // Depth helpers (number of finalized layers - 1)
    int depth_left() const { return static_cast<int>(left_.size()) - 1; }
    int depth_right() const { return static_cast<int>(right_.size()) - 1; }

    // Element iteration (root-first, layer order) for each side
    auto begin_left() const { return left_.begin(); }
    auto end_left() const { return left_.end(); }
    auto begin_right() const { return right_.begin(); }
    auto end_right() const { return right_.end(); }

private:
    LUT left_;
    LUT right_;
};

// Summary of a MITM match, including paths and a mapping between the two
// canonical representatives at the meeting point.
struct MITMMatchResult {
    bool found{false};
    SO6 meet{};                            // intersecting element (canonical)

    int dl{-1};                            // depth from left root to meet
    int dr{-1};                            // depth from right root to meet
    std::vector<uint8_t> left_path;        // T-sequence from left root to meet
    std::vector<uint8_t> right_path;       // T-sequence from right root to meet

    std::array<uint8_t,6> row_map{};       // row mapping from left to right
    std::array<uint8_t,6> col_map{};       // col mapping from left to right
    uint8_t sign_mask{0};                  // per-row sign flips to apply to left
    bool mapping_ok{false};                // true if reconcile_matrices succeeded
};

// Alternate expansions of the two sides until intersection or depth limit.
// Returns the meeting element if found; std::nullopt otherwise.
std::optional<SO6> generate_mitm_until_match(MITM& mitm);

// Run a MITM search and, if a meet is found, recover paths on both sides and
// a row/col/sign mapping between the two canonical representatives at the meet.
MITMMatchResult generate_mitm_match(MITM& mitm);

// Attempt to compute row/col/sign mapping that transforms lhs to rhs exactly.
bool reconcile_matrices(const SO6& lhs, const SO6& rhs,
                        std::array<uint8_t,6>& row_map,
                        std::array<uint8_t,6>& col_map,
                        uint8_t& sign_mask_out);

#endif // MITM_HPP
