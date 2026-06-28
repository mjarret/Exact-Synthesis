#pragma once
// ----------------------------------------------------------------------------
// TT alphabet: the set of "double-T" moves used by the TT LUT generator.
//
// A TT move is an ordered pair (first, second) of individual T operators, applied
// as T(second) * T(first) * S. There are 15 individual T operators (the 15 row
// pairs of C(6,2), indexed exactly as T_Operator::index / pair_index()).
//
// Equivalence rules that shrink the 210 ordered distinct pairs to 165 unique moves:
//   - first == second is excluded (T_i^2 = I).
//   - DISJOINT pairs (4 distinct rows) commute, so (a,b) == (b,a); keep one canonical
//     ordering (first < second). -> 45 unordered disjoint moves.
//   - OVERLAPPING pairs (3 distinct rows, one shared) do NOT commute; keep both
//     orderings. -> 120 ordered overlapping moves.
//   Total: 45 + 120 = 165.
//
// Pruning (exact-depth immediate-backtracking), expressed purely over TT moves:
// given the parent TT move, let L be its SECOND individual T (the last T on the path;
// L = 15 sentinel at the root). A candidate move is illegal iff
//   - candidate.first == L                 (the new first T cancels the previous last T), or
//   - candidate.disjoint && candidate.second == L  (disjoint commute collapses the suffix).
// The overlapping case keeps second == L (orderings genuinely differ). This removes
// exactly 14 moves for any L in 0..14 (8 overlapping with first==L, plus all 6 disjoint
// partners of L split between first==L and second==L), leaving 151; the root keeps 165.
// ----------------------------------------------------------------------------

#include <array>
#include <cstdint>
#include <bit>
#include <utility>

namespace tt {

// Individual-T index -> its (row1,row2). Matches T_Operator/compute_index ordering.
inline constexpr std::array<std::array<uint8_t, 2>, 15> kPairOf = {{
    {{0, 1}}, {{0, 2}}, {{0, 3}}, {{0, 4}}, {{0, 5}},
    {{1, 2}}, {{1, 3}}, {{1, 4}}, {{1, 5}},
    {{2, 3}}, {{2, 4}}, {{2, 5}},
    {{3, 4}}, {{3, 5}},
    {{4, 5}}
}};

inline constexpr uint8_t kSentinelLast = 15;  // "no previous T" (root)
inline constexpr uint8_t kNoMove       = 255; // SO6::last_TT sentinel ("not TT-generated")
inline constexpr std::size_t kNumMoves = 165;

struct TTMove {
    uint8_t first;     // 0..14
    uint8_t second;    // 0..14
    uint8_t rows[4];   // distinct affected rows, ascending; 0xFF pad (overlapping uses 3)
    bool    disjoint;  // true iff the two pairs touch 4 distinct rows (they commute)
};

namespace detail {
constexpr uint8_t rowmask(uint8_t t) {
    return static_cast<uint8_t>((1u << kPairOf[t][0]) | (1u << kPairOf[t][1]));
}
constexpr bool is_disjoint(uint8_t f, uint8_t s) {
    return (rowmask(f) & rowmask(s)) == 0;
}
constexpr TTMove make_move(uint8_t f, uint8_t s) {
    TTMove m{};
    m.first = f;
    m.second = s;
    m.disjoint = is_disjoint(f, s);
    const uint8_t mask = static_cast<uint8_t>(rowmask(f) | rowmask(s));
    m.rows[0] = m.rows[1] = m.rows[2] = m.rows[3] = 0xFF;
    int n = 0;
    for (uint8_t r = 0; r < 6; ++r)
        if (mask & (1u << r)) m.rows[n++] = r;
    return m;
}
} // namespace detail

// The 165 unique TT moves. Order is deterministic (first, then second), so move ids
// are stable. Disjoint moves are kept only in canonical first<second orientation.
inline constexpr std::array<TTMove, kNumMoves> kAlphabet = [] {
    std::array<TTMove, kNumMoves> a{};
    std::size_t n = 0;
    for (uint8_t f = 0; f < 15; ++f)
        for (uint8_t s = 0; s < 15; ++s) {
            if (f == s) continue;
            if (detail::is_disjoint(f, s) && f > s) continue; // commute: keep f<s only
            a[n++] = detail::make_move(f, s);
        }
    return a;
}();

// (first,second) -> move id, or 0xFF when (first,second) is not a kept move.
inline constexpr std::array<std::array<uint8_t, 15>, 15> kMoveId = [] {
    std::array<std::array<uint8_t, 15>, 15> t{};
    for (auto& row : t) row.fill(0xFF);
    for (std::size_t i = 0; i < kAlphabet.size(); ++i)
        t[kAlphabet[i].first][kAlphabet[i].second] = static_cast<uint8_t>(i);
    return t;
}();

// Index into kLegalByPrevMove meaning "no parent move" (root): no pruning.
inline constexpr uint8_t kRootPrev = 165;

// Legal candidate moves after a given parent TT move, grouped by the candidate's first T
// so the generator applies the shared first T once per group and fans out over seconds.
struct LegalGroups {
    std::array<std::array<uint8_t, 14>, 15> moves{}; // moves[first][0..count[first])
    std::array<uint8_t, 15> count{};                 // legal moves per first T
    uint16_t total{0};
};

// kLegalByPrevMove[p] (p = parent move id 0..164, or kRootPrev=165 for the root) lists the
// candidate moves that genuinely advance the T-depth by 2. A candidate (f,s) appended to a
// path ending in the parent (pf,ps) is REJECTED iff the 4-T suffix `pf ps f s` is
// non-geodesic, i.e. it reduces so the candidate lands at depth <= 2k-2 (a guaranteed
// duplicate -- "depth(parent . candidate) <= depth(parent)"). The reductions detectable
// from those 4 T's (using T_i^2 = I and commutation of disjoint T's):
//   (a) f == ps                                     -- new first cancels parent last
//   (b) s == ps && disjoint(f,s)                    -- commute f,s; s then cancels parent last
//   (c) f == pf && disjoint(pf,ps)                  -- commute new-first past ps; cancels parent first
//   (d) s == pf && disjoint(f,s) && disjoint(pf,ps) -- commute s past f,ps; cancels parent first
// (c)/(d) require the parent move to be disjoint, and crucially require the parent's FIRST T
// (pf) -- which is why pruning is keyed on the full parent move, not just its last T.
// Rejecting these never drops a real depth-2k matrix: any such matrix still has a geodesic
// 2-T extension from some parent, and a geodesic suffix is not rejected.
inline constexpr std::array<LegalGroups, 166> kLegalByPrevMove = [] {
    std::array<LegalGroups, 166> out{};
    for (std::size_t p = 0; p < 166; ++p) {
        const bool root = (p == kRootPrev);
        const uint8_t pf = root ? 0 : kAlphabet[p].first;
        const uint8_t ps = root ? 0 : kAlphabet[p].second;
        const bool prev_disjoint = !root && detail::is_disjoint(pf, ps);
        LegalGroups g{};
        for (std::size_t i = 0; i < kAlphabet.size(); ++i) {
            const TTMove& m = kAlphabet[i];
            if (!root) {
                const uint8_t f = m.first, s = m.second;
                // (a)-(d) reject non-geodesic compositions (the candidate's T's cancel into
                // the parent so it lands at depth <= 2k-2); these are not visible to the
                // ordering check below because that assumes a geodesic word.
                if (f == ps) continue;                                    // (a)
                if (m.disjoint && s == ps) continue;                      // (b)
                if (prev_disjoint && f == pf) continue;                   // (c)
                if (prev_disjoint && m.disjoint && s == pf) continue;     // (d)
                // NOTE: commutation-ORDER (normal-form) pruning is NOT sound here. The
                // overlapping T's satisfy braid relations (this is not a right-angled
                // group), and the "same matrix via the swapped ordering from a different
                // parent" route is not guaranteed to be explored (that intermediate may be
                // pruned), so reordering-based rejects drop real matrices -- verified: they
                // fail the random-root depth-6 LUT-equivalence gate. Only the geodesic
                // reductions (a)-(d), which truly lower the depth, are kept.
            }
            g.moves[m.first][g.count[m.first]++] = static_cast<uint8_t>(i);
            ++g.total;
        }
        out[p] = g;
    }
    return out;
}();

// ---- compile-time validation of the alphabet and pruning counts ----
namespace detail {
constexpr int count_disjoint() {
    int c = 0;
    for (const auto& m : kAlphabet) if (m.disjoint) ++c;
    return c;
}
constexpr bool no_self_pairs() {
    for (const auto& m : kAlphabet) if (m.first == m.second) return false;
    return true;
}
constexpr bool unique_pairs() {
    std::array<std::array<bool, 15>, 15> seen{};
    for (const auto& m : kAlphabet) {
        if (seen[m.first][m.second]) return false;
        seen[m.first][m.second] = true;
    }
    return true;
}
constexpr bool every_parent_prunes() {
    // Every non-root parent must reject at least the boundary cases, and never all moves.
    for (std::size_t p = 0; p < kAlphabet.size(); ++p) {
        const uint16_t t = kLegalByPrevMove[p].total;
        if (t == 0 || t >= 165) return false;
    }
    return true;
}
} // namespace detail

static_assert(kAlphabet.size() == 165, "TT alphabet must have 165 moves");
static_assert(detail::count_disjoint() == 45, "expected 45 disjoint TT moves");
static_assert(165 - detail::count_disjoint() == 120, "expected 120 overlapping TT moves");
static_assert(detail::no_self_pairs(), "no TT move may repeat the same T");
static_assert(detail::unique_pairs(), "TT moves (first,second) must be unique");
static_assert(kLegalByPrevMove[kRootPrev].total == 165, "root must allow all 165 moves");
static_assert(detail::every_parent_prunes(), "each parent must prune some (but not all) moves");

} // namespace tt
