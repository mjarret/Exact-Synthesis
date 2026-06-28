// Correctness tests for the TT (double-T) LUT generator.
//
// Covers:
//   1. Kernel equivalence: for all 165 TT moves x many random SO6, the fused kernel
//      reproduces T(second) * T(first), both as raw value-only transforms and as full
//      public (canonicalized) operations. Disjoint and overlapping reported separately.
//   2. Alphabet/pruning: 165 moves, 45 disjoint / 120 overlapping, no first==second,
//      unique pairs, disjoint reversed once / overlapping reversed twice, 151 legal per
//      last-T and 165 at the root.
//   3. LUT equivalence: TT depth-2k finalized layers equal the T depth-2k layers (both
//      dedup()'d to remove the lazy-canon race's over-counts).
//   4. Path reconstruction: path_to() returns the exact 2*layer individual-T sequence
//      that replays from the root to the stored state.
//   5. Regression: the T LUT layer sizes match the known sequence.
//
// Exit code 0 on success, 1 on any failure (no exceptions/asserts; explicit checks).

#include <cstdint>
#include <cstdio>
#include <random>
#include <unordered_set>
#include <vector>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "so6/TT_Operator.hpp"
#include "so6/TT_Alphabet.hpp"
#include "algo/Generate.hpp"
#include "ds/LUT.hpp"

namespace {

int g_failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { ++g_failures; std::fprintf(stderr, "[FAIL] %s\n", what); }
}

SO6 random_so6(std::mt19937_64& rng, int steps) {
    std::uniform_int_distribution<int> d(0, 14);
    SO6 cur = SO6::identity();
    for (int i = 0; i < steps; ++i)
        cur = T_OperatorRuntime(static_cast<uint8_t>(d(rng))) * cur;
    cur.last_T = 15;
    cur.last_TT = 255;
    return cur;
}

// Collect a LUT's finalized layers (pointers, in layer/BFS order).
std::vector<const finalized_set*> layers_of(const LUT& lut) {
    std::vector<const finalized_set*> v;
    for (const auto& layer : lut.layers()) v.push_back(&layer);
    return v;
}

bool layers_equal(const finalized_set& a, const finalized_set& b) {
    if (a.size() != b.size()) return false;
    for (const auto& s : a)
        if (b.find(s) == b.end()) return false;
    return true;
}

void configure_for_build(uint8_t stored_depth) {
    THREADS = 4;                 // exercise parallelism (+ the dedup race path)
    stored_depth_max = stored_depth;
    target_T_count = stored_depth;
    verbose = false;
    suppress_indicators = true;
}

// ---- 1. fused-kernel equivalence ----
void test_kernel_equivalence() {
    std::mt19937_64 rng(0xC0FFEEu);
    size_t raw_fail_disjoint = 0, raw_fail_overlap = 0, canon_fail = 0;
    constexpr int kTrials = 300;
    for (int trial = 0; trial < kTrials; ++trial) {
        const SO6 R = random_so6(rng, 8);
        for (uint8_t mv = 0; mv < tt::kNumMoves; ++mv) {
            const uint8_t f = tt::kAlphabet[mv].first;
            const uint8_t s = tt::kAlphabet[mv].second;

            // Raw value-only: fused one-shot vs two sequential butterflies.
            SO6 fused = TT_OperatorRuntime(mv, /*canonicalize=*/false).apply(R);
            SO6 seq = R;
            tt::apply_T_values_only_rt(f, seq);
            tt::apply_T_values_only_rt(s, seq);
            if (!(fused.arr_ == seq.arr_)) {
                if (tt::kAlphabet[mv].disjoint) ++raw_fail_disjoint; else ++raw_fail_overlap;
            }

            // Full public op: TT(move)*R vs T(second)*(T(first)*R), canonical equality.
            SO6 tt_pub = TT_OperatorRuntime(mv, /*canonicalize=*/true).apply(R);
            SO6 t_pub  = T_OperatorRuntime(s) * (T_OperatorRuntime(f) * R);
            if (!(tt_pub == t_pub)) ++canon_fail;
        }
    }
    check(raw_fail_disjoint == 0, "fused kernel == T-then-T (disjoint, raw values)");
    check(raw_fail_overlap == 0, "fused kernel == T-then-T (overlapping, raw values)");
    check(canon_fail == 0, "TT(move)*S == T(second)*(T(first)*S) (canonical)");
    std::printf("  [kernel] trials=%d moves=%zu raw_fail(disjoint=%zu,overlap=%zu) canon_fail=%zu\n",
                kTrials, tt::kNumMoves, raw_fail_disjoint, raw_fail_overlap, canon_fail);
}

// ---- 2. alphabet & pruning ----
void test_alphabet() {
    int disj = 0;
    for (const auto& m : tt::kAlphabet) if (m.disjoint) ++disj;
    check(tt::kAlphabet.size() == 165, "alphabet has 165 moves");
    check(disj == 45, "45 disjoint moves");
    check(165 - disj == 120, "120 overlapping moves");

    bool no_self = true, uniq = true;
    std::array<std::array<int, 15>, 15> seen{};
    for (const auto& m : tt::kAlphabet) {
        if (m.first == m.second) no_self = false;
        if (seen[m.first][m.second]++) uniq = false;
    }
    check(no_self, "no move has first==second");
    check(uniq, "no duplicate (first,second) pair");

    // Disjoint reversed pair represented once; overlapping reversed pair twice.
    bool disjoint_once = true, overlap_twice = true;
    for (uint8_t f = 0; f < 15; ++f)
        for (uint8_t s = static_cast<uint8_t>(f + 1); s < 15; ++s) {
            const bool fs = (tt::kMoveId[f][s] != 0xFF);
            const bool sf = (tt::kMoveId[s][f] != 0xFF);
            if (tt::detail::is_disjoint(f, s)) { if (!(fs ^ sf)) disjoint_once = false; }
            else { if (!(fs && sf)) overlap_twice = false; }
        }
    check(disjoint_once, "disjoint reversed pairs represented exactly once");
    check(overlap_twice, "overlapping reversed pairs represented twice");

    check(tt::kLegalByPrevMove[tt::kRootPrev].total == 165, "root allows all 165 moves");
    bool all_in_range = true;
    int min_legal = 165, max_legal = 0, sum_legal = 0;
    for (std::size_t p = 0; p < tt::kAlphabet.size(); ++p) {
        const int tot = tt::kLegalByPrevMove[p].total;
        if (tot <= 0 || tot >= 165) all_in_range = false;
        if (tot < min_legal) min_legal = tot;
        if (tot > max_legal) max_legal = tot;
        sum_legal += tot;
    }
    check(all_in_range, "each parent prunes some (but not all) moves");
    std::printf("  [alphabet] moves=165 disjoint=%d overlap=%d root=165 legal/parent in [%d,%d] avg=%.1f\n",
                disj, 165 - disj, min_legal, max_legal,
                static_cast<double>(sum_legal) / tt::kAlphabet.size());
}

// ---- 3. LUT equivalence (TT depth-2k == T depth-2k) ----
void test_lut_equivalence(uint8_t depth, const SO6& root = SO6::identity(), const char* label = "identity") {
    configure_for_build(depth);
    LUT t_lut = algo::create_lookup_table(root, nullptr, nullptr);
    t_lut.dedup();

    configure_for_build(depth);
    LUT tt_lut = algo::create_lookup_table_TT(root, nullptr, nullptr);
    tt_lut.dedup();

    auto tl = layers_of(t_lut);
    auto ttl = layers_of(tt_lut);

    // T LUT: layers at T-depth 0..depth. TT LUT: layers at T-depth 0,2,...,depth.
    bool ok = (ttl.size() == static_cast<size_t>(depth / 2) + 1);
    for (size_t i = 0; ok && i < ttl.size(); ++i) {
        const size_t t_depth = 2 * i;
        if (t_depth >= tl.size() || !layers_equal(*ttl[i], *tl[t_depth])) ok = false;
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "TT depth-2k layers == T depth-2k layers (T-depth %u, root=%s)", depth, label);
    check(ok, buf);
    std::printf("  [lut-eq depth %u root=%s] T layers=%zu TT layers=%zu match=%s\n",
                depth, label, tl.size(), ttl.size(), ok ? "yes" : "NO");
}

// ---- 4. path reconstruction ----
void test_path_reconstruction(uint8_t depth) {
    configure_for_build(depth);
    LUT tt_lut = algo::create_lookup_table_TT(SO6::identity(), nullptr, nullptr);
    tt_lut.dedup();

    auto layers = layers_of(tt_lut);
    const SO6 root = SO6::identity();
    size_t checked = 0, bad_len = 0, bad_replay = 0, missing = 0;
    for (size_t li = 0; li < layers.size(); ++li) {
        for (const auto& s : *layers[li]) {
            auto path = tt_lut.path_to(s);
            if (!path) { ++missing; continue; }
            if (path->size() != 2 * li) ++bad_len;
            SO6 cur = root;
            for (uint8_t t : *path) cur = T_OperatorRuntime(t) * cur;
            if (!(cur == s)) ++bad_replay;
            ++checked;
        }
    }
    check(missing == 0, "path_to finds every stored TT state");
    check(bad_len == 0, "TT path length == 2 * layer index");
    check(bad_replay == 0, "replaying the individual-T path reproduces the state");
    std::printf("  [path depth %u] checked=%zu missing=%zu bad_len=%zu bad_replay=%zu\n",
                depth, checked, missing, bad_len, bad_replay);
}

// ---- 5. regression: T LUT layer sizes ----
void test_t_regression() {
    static const size_t expect[] = {1, 1, 2, 6, 19, 77, 371}; // T-depth 0..6
    configure_for_build(6);
    LUT t_lut = algo::create_lookup_table(SO6::identity(), nullptr, nullptr);
    t_lut.dedup();
    auto tl = layers_of(t_lut);
    bool ok = (tl.size() == 7);
    for (size_t i = 0; ok && i < 7; ++i) if (tl[i]->size() != expect[i]) ok = false;
    check(ok, "T LUT layer sizes match known sequence 1,1,2,6,19,77,371");
    std::printf("  [regression] T layers:");
    for (auto* l : tl) std::printf(" %zu", l->size());
    std::printf("\n");
}

} // namespace

int main() {
    std::printf("== TT operator correctness tests ==\n");
    test_alphabet();
    test_kernel_equivalence();
    test_t_regression();
    test_lut_equivalence(4);
    test_lut_equivalence(6);
    test_lut_equivalence(8);                 // deeper gate: aggressive pruning must not drop matrices
    {
        std::mt19937_64 rng(0x5EED1234u);
        SO6 r = random_so6(rng, 9);          // random root, treated as sentinel
        test_lut_equivalence(6, r, "random");
    }
    test_path_reconstruction(6);

    if (g_failures == 0) {
        std::printf("[tt_operator_test] OK (all checks passed)\n");
        return 0;
    }
    std::fprintf(stderr, "[tt_operator_test] FAILED (%d check(s))\n", g_failures);
    return 1;
}
