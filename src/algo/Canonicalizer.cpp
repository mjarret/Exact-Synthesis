#include "so6/SO6.hpp"
#include "util/utils.hpp"
#include "sort/sort6.hpp"
#include <algorithm>
#include <array>
#include <vector>
#include <map>
#include <concepts>
#include <span>
#include "ds/SignatureMaskMap.hpp"
#include "ds/Lehmer6.hpp"

using std::strong_ordering;
constexpr auto Equal = std::strong_ordering::equal;
constexpr auto Less = std::strong_ordering::less;
constexpr auto Greater = std::strong_ordering::greater;

namespace {
    // Minimal concept for permutation-like objects of 6 uint8_t elements.
    // Intentionally simple: index access and size() are required; contiguous
    // storage is NOT required. Raw pointers will continue to bind to the
    // non-templated overloads to preserve current fast paths.
    template <class P>
    concept PermutationLike6 = requires(const P& p) {
        { p.size() } -> std::convertible_to<std::size_t>;
        { p[0] } -> std::convertible_to<uint8_t>;
        { p[1] } -> std::convertible_to<uint8_t>;
        { p[2] } -> std::convertible_to<uint8_t>;
        { p[3] } -> std::convertible_to<uint8_t>;
        { p[4] } -> std::convertible_to<uint8_t>;
        { p[5] } -> std::convertible_to<uint8_t>;
    };

    // Local alias to make the storage type for permutations easy to swap later
    using PermBuffer = std::array<uint8_t, 6>;

    // Load a packed 24-bit element from column base and row (3 bytes per entry)
    inline __attribute__((always_inline)) uint32_t load24(const uint8_t* base_col, uint8_t row) {
        const uint8_t* p = base_col + static_cast<int>(row) * 3;
        return static_cast<uint32_t>(p[0])
             | (static_cast<uint32_t>(p[1]) << 8)
             | (static_cast<uint32_t>(p[2]) << 16);
    }

    // Raw lexicographic order for two columns within the same matrix under a single sign mask.
    // Returns the same strong_ordering semantics as utils::lex_order (comparison of right vs left).
    inline __attribute__((always_inline)) strong_ordering lex_order_col_rowonly_raw(
        const SO6& s, const uint8_t* row_a, int colL, int colR, uint16_t sign_mask)
    {
        const uint8_t* baseL = s.arr24_ + static_cast<int>(colL) * 18;
        const uint8_t* baseR = s.arr24_ + static_cast<int>(colR) * 18;
        uint16_t smL = sign_mask;
        uint16_t smR = sign_mask;

        int i = 0;
        strong_ordering comp1 = Equal;
        strong_ordering comp2 = Equal;

        // Phase 1: find orientation
        for (; i < 6; ++i) {
            const uint32_t Ld = load24(baseL, row_a[i]);
            const uint32_t Rd = load24(baseR, row_a[i]);
            const int8_t Lic = static_cast<int8_t>(Ld & 0xFF);
            const int8_t Ric = static_cast<int8_t>(Rd & 0xFF);
            comp1 = (Lic < 0) ? Less : (Lic > 0 ? Greater : Equal);
            comp2 = (Ric < 0) ? Less : (Ric > 0 ? Greater : Equal);
            if (comp1 == Equal && comp2 == Equal) continue;
            if (comp1 == Equal) return Greater;
            if (comp2 == Equal) return Less;
            const uint8_t fsm = static_cast<uint8_t>((smL >> i) & utils::BITS);
            const uint8_t ssm = static_cast<uint8_t>((smR >> i) & utils::BITS);
            if ((comp1 == Less) ^ (fsm == utils::NEG)) smL ^= 0x3Fu;
            if ((comp2 == Less) ^ (ssm == utils::NEG)) smR ^= 0x3Fu;
            break;
        }

        // Phase 2: compare with signs applied
        for (; i < 6; ++i) {
            const uint32_t Ld = load24(baseL, row_a[i]);
            const uint32_t Rd = load24(baseR, row_a[i]);
            const uint16_t Lnum = static_cast<uint16_t>(Ld & 0xFFFFu);
            const uint16_t Rnum = static_cast<uint16_t>(Rd & 0xFFFFu);
            const bool first_is_neg  = (((smL >> i) & utils::BITS) == utils::NEG);
            const bool second_is_neg = (((smR >> i) & utils::BITS) == utils::NEG);

            const uint16_t Lnum_eff = first_is_neg  ? static_cast<uint16_t>((Lnum != 0) ? (256u - Lnum) : 0u) : Lnum;
            const uint16_t Rnum_eff = second_is_neg ? static_cast<uint16_t>((Rnum != 0) ? (256u - Rnum) : 0u) : Rnum;
            const uint32_t Leff = (Ld & 0xFF0000u) | Lnum_eff;
            const uint32_t Reff = (Rd & 0xFF0000u) | Rnum_eff;

            const uint32_t Lval = (static_cast<uint8_t>(Lnum_eff & 0xFFu) != 0) ? Leff : 0u;
            const uint32_t Rval = (static_cast<uint8_t>(Rnum_eff & 0xFFu) != 0) ? Reff : 0u;

            if (Rval == Lval) continue;
            if (static_cast<int8_t>(Lnum & 0xFFu) == 0) return Greater;
            if (static_cast<int8_t>(Rnum & 0xFFu) == 0) return Less;
            return (Rval < Lval) ? Less : Greater;
        }
        return Equal;
    }

    // Contiguous view overload: forward to pointer fast-path
    inline __attribute__((always_inline)) strong_ordering lex_order_col_rowonly_raw(
        const SO6& s, std::span<const uint8_t, 6> row, int colL, int colR, uint16_t sign_mask)
    {
        return lex_order_col_rowonly_raw(s, row.data(), colL, colR, sign_mask);
    }

    // Generic overload accepting any PermutationLike6. This preserves the
    // existing pointer-based fast path by providing a separate template
    // overload rather than changing call sites. When the caller provides a
    // non-pointer permutation object (e.g., std::array or a custom type),
    // this overload will be selected and index into it directly.
    template <PermutationLike6 Row>
    inline __attribute__((always_inline)) strong_ordering lex_order_col_rowonly_raw(
        const SO6& s, const Row& row, int colL, int colR, uint16_t sign_mask)
    {
        const uint8_t* baseL = s.arr24_ + static_cast<int>(colL) * 18;
        const uint8_t* baseR = s.arr24_ + static_cast<int>(colR) * 18;
        uint16_t smL = sign_mask;
        uint16_t smR = sign_mask;

        int i = 0;
        strong_ordering comp1 = Equal;
        strong_ordering comp2 = Equal;

        // Phase 1: find orientation
        for (; i < 6; ++i) {
            const uint32_t Ld = load24(baseL, static_cast<uint8_t>(row[static_cast<size_t>(i)]));
            const uint32_t Rd = load24(baseR, static_cast<uint8_t>(row[static_cast<size_t>(i)]));
            const int8_t Lic = static_cast<int8_t>(Ld & 0xFF);
            const int8_t Ric = static_cast<int8_t>(Rd & 0xFF);
            comp1 = (Lic < 0) ? Less : (Lic > 0 ? Greater : Equal);
            comp2 = (Ric < 0) ? Less : (Ric > 0 ? Greater : Equal);
            if (comp1 == Equal && comp2 == Equal) continue;
            if (comp1 == Equal) return Greater;
            if (comp2 == Equal) return Less;
            const uint8_t fsm = static_cast<uint8_t>((smL >> i) & utils::BITS);
            const uint8_t ssm = static_cast<uint8_t>((smR >> i) & utils::BITS);
            if ((comp1 == Less) ^ (fsm == utils::NEG)) smL ^= 0x3Fu;
            if ((comp2 == Less) ^ (ssm == utils::NEG)) smR ^= 0x3Fu;
            break;
        }

        // Phase 2: compare with signs applied
        for (; i < 6; ++i) {
            const uint32_t Ld = load24(baseL, static_cast<uint8_t>(row[static_cast<size_t>(i)]));
            const uint32_t Rd = load24(baseR, static_cast<uint8_t>(row[static_cast<size_t>(i)]));
            const uint16_t Lnum = static_cast<uint16_t>(Ld & 0xFFFFu);
            const uint16_t Rnum = static_cast<uint16_t>(Rd & 0xFFFFu);
            const bool first_is_neg  = (((smL >> i) & utils::BITS) == utils::NEG);
            const bool second_is_neg = (((smR >> i) & utils::BITS) == utils::NEG);

            const uint16_t Lnum_eff = first_is_neg  ? static_cast<uint16_t>((Lnum != 0) ? (256u - Lnum) : 0u) : Lnum;
            const uint16_t Rnum_eff = second_is_neg ? static_cast<uint16_t>((Rnum != 0) ? (256u - Rnum) : 0u) : Rnum;
            const uint32_t Leff = (Ld & 0xFF0000u) | Lnum_eff;
            const uint32_t Reff = (Rd & 0xFF0000u) | Rnum_eff;

            const uint32_t Lval = (static_cast<uint8_t>(Lnum_eff & 0xFFu) != 0) ? Leff : 0u;
            const uint32_t Rval = (static_cast<uint8_t>(Rnum_eff & 0xFFu) != 0) ? Reff : 0u;

            if (Rval == Lval) continue;
            if (static_cast<int8_t>(Lnum & 0xFFu) == 0) return Greater;
            if (static_cast<int8_t>(Rnum & 0xFFu) == 0) return Less;
            return (Rval < Lval) ? Less : Greater;
        }
        return Equal;
    }

    struct PermView {
        using value_type = uint8_t;
        uint8_t* data;
        size_t n;
        size_t size() const { return n; }
        uint8_t& operator[](size_t i) { return data[i]; }
        const uint8_t& operator[](size_t i) const { return data[i]; }
        uint8_t* begin() { return data; }
        uint8_t* end() { return data + n; }
    };

    // Flat, stack-only replacement for FrequencyTable when desired
    struct FlatFrequencyTable {
        uint16_t sig[6]{};
        order6::Order6 ord[6]{};
        uint8_t n{0};

        void clear() { n = 0; }
        uint8_t size() const { return n; }

        void push(uint16_t s, uint8_t mask) {
            sig[n] = s;
            order6::Order6 o{}; o.set_mask_rank(mask, 0); // ascending within block
            ord[n] = o;
            ++n;
        }

        int find(uint16_t s) const {
            for (uint8_t i = 0; i < n; ++i) if (sig[i] == s) return i;
            return -1;
        }
    };

    static FlatFrequencyTable build_row_flat(const SO6& s) {
        ds::SignatureMaskMap m;
        for (uint8_t row = 0; row < 6; ++row) m[SO6::row_frequency_signature(s, row)] |= row;
        // Order of blocks does not matter for this structure; consumers sort keys explicitly.
        FlatFrequencyTable ft;
        for (std::size_t i = 0; i < m.size(); ++i) ft.push(m[i].sig, m[i].mask);
        return ft;
    }

    static FlatFrequencyTable build_col_flat(const SO6& s) {
        ds::SignatureMaskMap m;
        for (uint8_t col = 0; col < 6; ++col) m[SO6::col_frequency_signature(s, col)] |= col;
        FlatFrequencyTable ft;
        for (std::size_t i = 0; i < m.size(); ++i) ft.push(m[i].sig, m[i].mask);
        return ft;
    }

    static std::vector<FrequencySignature> gather_sorted_keys(const FlatFrequencyTable& ft) {
        std::vector<FrequencySignature> keys; keys.reserve(ft.size());
        for (uint8_t i = 0; i < ft.size(); ++i) keys.push_back(ft.sig[i]);
        sort6::sorting_network_dispatch(keys);
        return keys;
    }

    static void materialize_permutation(const FlatFrequencyTable& ft,
                                        const std::vector<FrequencySignature>& keys,
                                        uint8_t* out) {
        uint8_t* write = out;
        uint8_t buf[6];
        for (auto k : keys) {
            int idx = ft.find(k);
            if (idx >= 0) {
                ft.ord[idx].to_array(buf);
                uint8_t mask = static_cast<uint8_t>(ft.ord[idx].mask());
                // number of set bits = block size
                uint8_t cnt = 0; for (uint8_t b = 0; b < 6; ++b) if (mask & (1u << b)) ++cnt;
                for (uint8_t j = 0; j < cnt; ++j) *write++ = buf[j];
            }
        }
    }

    static bool get_next_equivalence_class(FlatFrequencyTable& ft) {
        // Iterate in signature-sorted order for determinism
        std::vector<FrequencySignature> keys; keys.reserve(ft.size());
        for (uint8_t i = 0; i < ft.size(); ++i) keys.push_back(ft.sig[i]);
        sort6::sorting_network_dispatch(keys);
        for (auto s : keys) {
            int i = ft.find(s);
            if (i >= 0 && ft.ord[i].next_permutation()) return true;
            if (i >= 0) ft.ord[i].reset();
        }
        return false;
    }
    struct Blocks {
        struct Block {
            uint8_t mask{0};
            uint8_t size{0};
            order6::Order6 order{};   // current rank within this block
            uint8_t idx[6]{};         // ascending indices for this block
        };
        Block blocks[6]{};
        uint8_t count{0};

        // Build from a SignatureMaskMap sorted by signature (deterministic block order)
        void init_from_map(const ds::SignatureMaskMap& m) {
            count = static_cast<uint8_t>(m.size());
            for (uint8_t i = 0; i < count; ++i) {
                const auto &e = m[i];
                blocks[i].mask = e.mask;
                // extract indices in ascending order into idx
                uint8_t k = 0;
                for (uint8_t b = 0; b < 6; ++b) {
                    if (e.mask & (1u << b)) blocks[i].idx[k++] = b;
                }
                blocks[i].size = k;
                blocks[i].order = order6::Order6::from_array(blocks[i].idx, k); // rank 0
            }
        }

        // Concatenate block permutations into out[6] according to current orders
        void materialize(uint8_t out[6]) const {
            uint8_t* write = out;
            uint8_t buf[6];
            for (uint8_t i = 0; i < count; ++i) {
                blocks[i].order.to_array(buf);
                const uint8_t k = blocks[i].size;
                for (uint8_t j = 0; j < k; ++j) *write++ = buf[j];
            }
        }

        // Advance mixed-radix order across blocks; return true if advanced, false on full wrap
        bool next() {
            for (uint8_t i = 0; i < count; ++i) {
                if (blocks[i].order.next_permutation()) return true;
                blocks[i].order.reset();
            }
            return false;
        }
    };

    // Build row/col blocks directly from signatures (replaces temporary maps)
    static Blocks build_row_blocks(const SO6& s) {
        ds::SignatureMaskMap m;
        for (uint8_t row = 0; row < 6; ++row) m.add(SO6::row_frequency_signature(s, row), row);
        m.sort_by_signature();
        Blocks b; b.init_from_map(m); return b;
    }
    static Blocks build_col_blocks(const SO6& s) {
        ds::SignatureMaskMap m;
        for (uint8_t col = 0; col < 6; ++col) m.add(SO6::col_frequency_signature(s, col), col);
        m.sort_by_signature();
        Blocks b; b.init_from_map(m); return b;
    }

    // Precompute Order6 for all 6-bit masks (ascending index order -> rank 0)
    static const std::array<order6::Order6, 64>& mask_order_lut() {
        static const std::array<order6::Order6, 64> LUT = []{
            std::array<order6::Order6, 64> a{};
            for (uint16_t mask = 1; mask < 64; ++mask) {
                uint8_t idx[6];
                uint8_t k = 0;
                for (uint8_t b = 0; b < 6; ++b) {
                    if (mask & (1u << b)) idx[k++] = b;
                }
                a[mask] = order6::Order6::from_array(idx, k);
            }
            return a;
        }();
        return LUT;
    }

    std::vector<FrequencySignature> gather_sorted_keys(const FrequencyTable& ecs) {
        std::vector<FrequencySignature> keys;
        keys.reserve(6);
        for (const auto& kv : ecs) keys.push_back(kv.first);
        sort6::sorting_network_dispatch(keys);
        return keys;
    }

    std::vector<FrequencySignature> prepare_initial_permutation(FrequencyTable& ecs,
                                                                PermBuffer& buffer,
                                                                Lehmer6& perm_out) {
        auto keys = gather_sorted_keys(ecs);
        std::size_t w = 0;
        uint8_t tmp[6];
        for (auto key : keys) {
            auto it = ecs.find(key);
            const uint8_t len = it->second.size();
            it->second.to_array(tmp);
            for (uint8_t j = 0; j < len; ++j) buffer[w++] = tmp[j];
        }
        perm_out = Lehmer6::from_perm(buffer);
        return keys;
    }

    void materialize_permutation(const FrequencyTable& ecs,
                                 const std::vector<FrequencySignature>& keys,
                                 uint8_t* out) {
        uint8_t* write = out;
        uint8_t tmp[6];
        for (auto key : keys) {
            auto it = ecs.find(key);
            const uint8_t len = it->second.size();
            it->second.to_array(tmp);
            for (uint8_t j = 0; j < len; ++j) *write++ = tmp[j];
        }
    }

    template <typename Comparator>
    void reorder_and_materialize(FrequencyTable& ecs,
                                 const std::vector<FrequencySignature>& keys,
                                 Comparator&& comp,
                                 uint8_t* out) {
        uint8_t* write = out;
        uint8_t tmp[6];
        for (auto key : keys) {
            auto it = ecs.find(key);
            const uint8_t len = it->second.size();
            it->second.to_array(tmp);
            PermView view{tmp, len};
            sort6::sorting_network_dispatch(view, comp);
            it->second = order6::Order6::from_array(tmp, len);
            for (uint8_t j = 0; j < len; ++j) *write++ = tmp[j];
        }
    }
}

void SO6::canonical_form() {
    // Get equivalence classes and put into a consistent form
    auto row_ecs = row_equivalence_classes();
    PermBuffer row_perm{};
    auto row_keys = prepare_initial_permutation(row_ecs, row_perm, row_perm_lh_);

    auto col_ecs = col_equivalence_classes();
    PermBuffer col_perm{};
    auto col_keys = prepare_initial_permutation(col_ecs, col_perm, col_perm_lh_);


    do { 
        // Materialize candidate row permutation from equivalence classes
        materialize_permutation(row_ecs, row_keys, row_perm.data());
        // Use span view to stay abstract while hitting pointer fast-path
        const std::span<const uint8_t, 6> row_span{row_perm};

        // Loop over all possible sign conventions
        for(uint8_t sc = 0; sc < 32; ++sc) {
            auto comparator = [&](int i, int j) {
                return Less == lex_order_col_rowonly_raw(*this, row_span, i, j, sc);
            };

            reorder_and_materialize(col_ecs, col_keys, comparator, col_perm.data());

            // Compare using candidate arrays directly (current decoded inside is_better)
            if (is_better_permutation(row_perm.data(), col_perm.data(), sc)) {
                // Source of truth: assign Lehmer
                row_perm_lh_ = Lehmer6::from_perm(row_perm);
                col_perm_lh_ = Lehmer6::from_perm(col_perm);
                sign_convention = sc;
            }
        }
    }  while (get_next_equivalence_class(row_ecs));

}

FrequencyTable SO6::row_equivalence_classes() {
    // Faster: build a tiny signature->bitmask map, then emit Order6 per block
    ds::SignatureMaskMap m;
    for (uint8_t row = 0; row < 6; ++row) {
        const FrequencySignature sig = row_frequency_signature(*this, row);
        m[sig] |= row;
    }
    // Ensure deterministic block order: sort once by signature (n <= 6)
    FrequencyTable ret;
    ret.reserve(6);
    for (std::size_t i = 0; i < m.size(); ++i) {
        const auto &e = m[i];
        order6::Order6 o{};
        o.set_mask_rank(e.mask, 0); // ascending order within this block
        ret[e.sig] = o;
    }
    return ret;
}

FrequencyTable SO6::col_equivalence_classes() {
    ds::SignatureMaskMap m;
    for (uint8_t col = 0; col < 6; ++col) {
        const FrequencySignature sig = col_frequency_signature(*this, col);
        m[sig] |= col;
    }
    // Ensure deterministic block order: sort once by signature (n <= 6)
    FrequencyTable ret;
    ret.reserve(6);
    for (std::size_t i = 0; i < m.size(); ++i) {
        const auto &e = m[i];
        order6::Order6 o{};
        o.set_mask_rank(e.mask, 0);
        ret[e.sig] = o;
    }
    return ret;
}

bool SO6::get_next_equivalence_class(FrequencyTable& ecs) {
    return ecs.next_via_lut();
}
