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

    // Fixed-capacity, stack-only list of the distinct frequency signatures in a
    // matrix (at most 6). Drop-in replacement for the std::vector<FrequencySignature>
    // previously used during canonicalization: same API surface (reserve/push_back/
    // size/operator[]/begin/end) but no per-canonical_form heap allocation on the
    // hot path. Trivially copyable, so returning it by value is free.
    struct SigKeys {
        FrequencySignature data_[6]{};
        std::size_t n_ = 0;
        void reserve(std::size_t) {}                       // no-op; capacity fixed at 6
        void push_back(FrequencySignature v) { data_[n_++] = v; }
        std::size_t size() const { return n_; }
        FrequencySignature& operator[](std::size_t i) { return data_[i]; }
        const FrequencySignature& operator[](std::size_t i) const { return data_[i]; }
        FrequencySignature* begin() { return data_; }
        FrequencySignature* end() { return data_ + n_; }
        const FrequencySignature* begin() const { return data_; }
        const FrequencySignature* end() const { return data_ + n_; }
    };

    // Strong-order columns within the same matrix under a single sign mask,
    // using the existing lex_order helper (no packed-byte dependency).
    inline __attribute__((always_inline)) strong_ordering lex_order_col_rowonly_raw(
        const SO6& s, const uint8_t* row_a, int colL, int colR, uint16_t sign_mask)
    {
        struct ColIter {
            const SO6* s;
            const uint8_t* row;
            int col;
            int i;
            DyadicSqrt2 operator*() const {
                const int r = row ? row[i] : i;
                return s->get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(col));
            }
            ColIter& operator++() { ++i; return *this; }
            bool operator!=(const ColIter& other) const { return i != other.i; }
        };

        ColIter a_begin{&s, row_a, colL, 0};
        ColIter a_end  {&s, row_a, colL, 6};
        ColIter b_begin{&s, row_a, colR, 0};
        ColIter b_end  {&s, row_a, colR, 6};
        return utils::lex_order(a_begin, a_end, b_begin, b_end, sign_mask, sign_mask);
    }

    // Contiguous view overload: forward to pointer fast-path
    inline __attribute__((always_inline)) strong_ordering lex_order_col_rowonly_raw(
        const SO6& s, std::span<const uint8_t, 6> row, int colL, int colR, uint16_t sign_mask)
    {
        return lex_order_col_rowonly_raw(s, row.data(), colL, colR, sign_mask);
    }

    // Generic overload accepting any PermutationLike6.
    template <PermutationLike6 Row>
    inline __attribute__((always_inline)) strong_ordering lex_order_col_rowonly_raw(
        const SO6& s, const Row& row, int colL, int colR, uint16_t sign_mask)
    {
        uint8_t buf[6];
        for (int i = 0; i < 6; ++i) buf[i] = static_cast<uint8_t>(row[static_cast<std::size_t>(i)]);
        return lex_order_col_rowonly_raw(s, buf, colL, colR, sign_mask);
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
    SigKeys gather_sorted_keys(const FrequencyTable& ecs) {
        SigKeys keys;
        for (const auto& kv : ecs) keys.push_back(kv.first);
        sort6::sorting_network_dispatch(keys);
        return keys;
    }

    SigKeys prepare_initial_permutation(FrequencyTable& ecs,
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
                                 const SigKeys& keys,
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
                                 const SigKeys& keys,
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
