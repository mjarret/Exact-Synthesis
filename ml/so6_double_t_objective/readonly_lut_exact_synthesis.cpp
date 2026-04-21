#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <tbb/blocked_range.h>
#include <tbb/global_control.h>
#include <tbb/parallel_for.h>

#include "DyadicSqrt2.hpp"
#include "algo/Generate.hpp"
#include "config/Globals.hpp"
#include "ds/LUT.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "sys/memory.hpp"

namespace py = pybind11;

namespace {

using StateArray3 = py::array_t<word_t, py::array::c_style | py::array::forcecast>;
using AlphaArray2 = py::array_t<int64_t, py::array::c_style | py::array::forcecast>;

constexpr int kNumSingleT = 15;
constexpr int kNumDoubleT = 165;
constexpr int kMatrixSide = 6;
constexpr int kNumMatrixEntries = kMatrixSide * kMatrixSide;
constexpr int kNumMatrixFeatures = kNumMatrixEntries * 3;
constexpr int kPackedMaskWords = (kNumDoubleT + 63) / 64;

using PackedMask = std::array<uint64_t, kPackedMaskWords>;

const std::array<std::pair<uint8_t, uint8_t>, kNumSingleT> kTPairs = {{
    {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5},
    {1, 2}, {1, 3}, {1, 4}, {1, 5},
    {2, 3}, {2, 4}, {2, 5},
    {3, 4}, {3, 5},
    {4, 5},
}};

std::array<std::pair<uint8_t, uint8_t>, kNumDoubleT> build_tt_alphabet() {
    std::array<std::pair<uint8_t, uint8_t>, kNumDoubleT> out{};
    std::size_t idx = 0;
    for (uint8_t i = 0; i < kNumSingleT; ++i) {
        for (uint8_t j = 0; j < kNumSingleT; ++j) {
            if (i == j) continue;
            const auto [a0, a1] = kTPairs[i];
            const auto [b0, b1] = kTPairs[j];
            const bool disjoint = (a0 != b0) && (a0 != b1) && (a1 != b0) && (a1 != b1);
            if (disjoint) {
                if (i < j) out[idx++] = {i, j};
            } else {
                out[idx++] = {i, j};
            }
        }
    }
    if (idx != static_cast<std::size_t>(kNumDoubleT)) {
        throw std::runtime_error("TT alphabet construction failed: expected 165 entries");
    }
    return out;
}

const std::array<std::pair<uint8_t, uint8_t>, kNumDoubleT>& tt_alphabet() {
    static const auto alphabet = build_tt_alphabet();
    return alphabet;
}

std::size_t resolve_thread_count(int threads) {
    if (threads > 0) return threads;
    return std::thread::hardware_concurrency();
}

std::string format_seconds(double seconds) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(2);
    oss << seconds << "s";
    return oss.str();
}

std::string format_bytes(double bytes) {
    const double kib = 1024.0;
    const double mib = kib * 1024.0;
    const double gib = mib * 1024.0;
    const double tib = gib * 1024.0;

    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(2);
    if (bytes >= tib) {
        oss << (bytes / tib) << " TiB";
    } else if (bytes >= gib) {
        oss << (bytes / gib) << " GiB";
    } else if (bytes >= mib) {
        oss << (bytes / mib) << " MiB";
    } else if (bytes >= kib) {
        oss << (bytes / kib) << " KiB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

std::size_t rss_bytes() {
    return getProcessRSSBytes();
}

std::size_t rss_increase(std::size_t before) {
    const std::size_t after = rss_bytes();
    return after > before ? (after - before) : 0;
}

SO6 unpack_state(const word_t* ptr) {
    SO6 s;
    for (int r = 0; r < kMatrixSide; ++r) {
        for (int c = 0; c < kMatrixSide; ++c) {
            s.set_element(
                static_cast<uint8_t>(r),
                static_cast<uint8_t>(c),
                DyadicSqrt2(ptr[r * kMatrixSide + c])
            );
        }
    }
    s.last_T = 15;
    s.canonical_reset();
    return s;
}

void write_state_ptr(word_t* out_ptr, const SO6& s) {
    for (int r = 0; r < kMatrixSide; ++r) {
        for (int c = 0; c < kMatrixSide; ++c) {
            out_ptr[r * kMatrixSide + c] = s.get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c)).data;
        }
    }
}

void write_feature_ptr(float* out_ptr, const SO6& s) {
    std::size_t idx = 0;
    for (int r = 0; r < kMatrixSide; ++r) {
        for (int c = 0; c < kMatrixSide; ++c) {
            const DyadicSqrt2 z = s.get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c));
            out_ptr[idx++] = static_cast<float>(z.int_c);
            out_ptr[idx++] = static_cast<float>(z.sqrt2_c);
            out_ptr[idx++] = static_cast<float>(z.denom_exp);
        }
    }
}

inline SO6 apply_tt_pair(const SO6& input, const std::pair<uint8_t, uint8_t>& tt) {
    SO6 cur = T_OperatorRuntime(tt.first) * input;
    return T_OperatorRuntime(tt.second) * cur;
}

SO6 apply_tt(const SO6& input, int alpha_idx) {
    if (alpha_idx < 0 || alpha_idx >= kNumDoubleT) {
        throw std::runtime_error("alpha_idx out of range");
    }
    return apply_tt_pair(input, tt_alphabet()[static_cast<std::size_t>(alpha_idx)]);
}

std::vector<int> normalize_cached_depths(std::vector<int> depths, int max_t_depth) {
    if (depths.empty()) {
        for (int d = 2; d <= max_t_depth; d += 2) depths.push_back(d);
    }
    std::sort(depths.begin(), depths.end());
    depths.erase(std::unique(depths.begin(), depths.end()), depths.end());
    for (const int d : depths) {
        if (d < 0 || d > max_t_depth) {
            throw std::runtime_error("cached_depths contains a depth outside the built range");
        }
    }
    return depths;
}

std::vector<std::size_t> sample_indices(std::size_t size, int n, int seed) {
    std::vector<std::size_t> picks(static_cast<std::size_t>(n));
    if (n == 0) return picks;

    std::mt19937_64 rng(static_cast<uint64_t>(seed));
    std::uniform_int_distribution<std::size_t> dist(0, size - 1);
    for (int i = 0; i < n; ++i) {
        picks[static_cast<std::size_t>(i)] = dist(rng);
    }
    return picks;
}

void clear_packed_mask(PackedMask* mask) {
    for (auto& word : *mask) word = 0;
}

void set_packed_mask_bit(PackedMask* mask, int bit) {
    (*mask)[static_cast<std::size_t>(bit >> 6)] |= (uint64_t{1} << (bit & 63));
}

bool packed_mask_bit(const PackedMask& mask, int bit) {
    return ((mask[static_cast<std::size_t>(bit >> 6)] >> (bit & 63)) & uint64_t{1}) != 0;
}

void unpack_packed_mask_row(const PackedMask& mask, uint8_t* row) {
    for (int a = 0; a < kNumDoubleT; ++a) {
        row[a] = static_cast<uint8_t>(packed_mask_bit(mask, a));
    }
}

struct CachedLayer {
    bool available = false;
    std::vector<const SO6*> states;
    const finalized_set* target_layer = nullptr;
    std::unique_ptr<std::mutex> reducing_masks_mutex = std::make_unique<std::mutex>();
    mutable std::unordered_map<std::size_t, PackedMask> reducing_masks;
};

class ReadOnlyLUT {
public:
    ReadOnlyLUT(int max_t_depth = 12,
                int threads = 0,
                const std::string& progress_mode = "plain",
                bool verbose_build = true,
                bool debug = false,
                bool log_calls = false,
                std::vector<int> cached_depths = {})
        : max_t_depth_(max_t_depth),
          progress_mode_(progress_mode),
          verbose_build_(verbose_build),
          debug_(debug),
          log_calls_(log_calls),
          cached_depths_(normalize_cached_depths(std::move(cached_depths), max_t_depth)) {
        if (max_t_depth_ < 0) {
            throw std::runtime_error("max_t_depth must be nonnegative");
        }
        if (progress_mode_ != "bars" && progress_mode_ != "plain" && progress_mode_ != "off") {
            throw std::runtime_error("progress_mode must be one of: bars, plain, off");
        }

        resolved_threads_ = resolve_thread_count(threads);
        if (resolved_threads_ > 255) {
            resolved_threads_ = 255;
        }
        thread_control_ = std::make_unique<tbb::global_control>(
            tbb::global_control::max_allowed_parallelism,
            resolved_threads_
        );

        THREADS = static_cast<uint8_t>(resolved_threads_);
        target_T_count = static_cast<uint8_t>(max_t_depth_);
        stored_depth_max = static_cast<uint8_t>(max_t_depth_);
        verbose = verbose_build_;

        const auto build_start = std::chrono::steady_clock::now();
        const std::size_t rss0 = rss_bytes();
        log("[ctor] start");
        log("[ctor] requested max_t_depth=" + std::to_string(max_t_depth_)
            + " threads=" + std::to_string(resolved_threads_)
            + " progress_mode=" + progress_mode_
            + " cached_depths=" + cached_depths_string());

        suppress_indicators = (progress_mode_ != "bars");
        log("[ctor] calling algo::create_lookup_table"
            + std::string(progress_mode_ == "bars" ? " with indicator bars" : " with indicators off"));
        lut_ = algo::create_lookup_table(SO6::identity(), nullptr, nullptr);

        init_layer_refs();

        const auto build_end = std::chrono::steady_clock::now();
        build_seconds_ = std::chrono::duration<double>(build_end - build_start).count();
        log("[ctor] LUT build complete in " + format_seconds(build_seconds_)
            + " | rss delta=" + format_bytes(static_cast<double>(rss_increase(rss0))));

        const std::size_t rss1 = rss_bytes();
        cache_requested_layers();
        log("[ctor] cached requested sample layers | rss delta="
            + format_bytes(static_cast<double>(rss_increase(rss1))));

        if (verbose_build_) {
            std::cerr
                << "[readonly_lut] build finished in " << format_seconds(build_seconds_)
                << " | layers=" << lut_.size()
                << " | cached sample depths=" << cached_depths_string()
                << " | rss=" << format_bytes(static_cast<double>(rss_bytes()))
                << std::endl;
            std::cerr << "[readonly_lut] layer sizes:";
            const auto sizes = layer_sizes();
            for (std::size_t d = 0; d < sizes.size(); ++d) {
                std::cerr << (d == 0 ? " " : ", ") << d << ":" << sizes[d];
            }
            std::cerr << std::endl;
        }
    }

    std::size_t layer_size(int depth_t) const {
        check_depth(depth_t);
        return get_layer_ref(depth_t).size();
    }

    std::vector<std::size_t> layer_sizes() const {
        std::vector<std::size_t> out;
        out.reserve(layer_refs_.size());
        for (const finalized_set* layer : layer_refs_) {
            out.push_back(layer->size());
        }
        return out;
    }

    py::tuple sample_labeled_features_at_t_depth(int depth_t, int n, int seed) const {
        log_call("sample_labeled_features_at_t_depth", "depth=" + std::to_string(depth_t) + " n=" + std::to_string(n) + " seed=" + std::to_string(seed));
        if (n < 0) throw std::runtime_error("n must be nonnegative");

        py::array_t<float> features(std::vector<py::ssize_t>{
            static_cast<py::ssize_t>(n),
            static_cast<py::ssize_t>(kNumMatrixFeatures),
        });
        py::array_t<word_t> states(std::vector<py::ssize_t>{static_cast<py::ssize_t>(n), kMatrixSide, kMatrixSide});
        py::array_t<uint8_t> mask(std::vector<py::ssize_t>{static_cast<py::ssize_t>(n), static_cast<py::ssize_t>(kNumDoubleT)});
        if (n == 0) return py::make_tuple(std::move(features), std::move(states), std::move(mask));

        float* feature_ptr = features.mutable_data();
        word_t* states_ptr = states.mutable_data();
        uint8_t* mask_ptr = mask.mutable_data();
        fill_sampled_features_states_and_masks(
            depth_t,
            n,
            seed,
            feature_ptr,
            mask_ptr,
            [&](std::size_t i, const SO6& source, std::size_t) {
                write_state_ptr(states_ptr + i * kNumMatrixEntries, source);
            }
        );

        return py::make_tuple(std::move(features), std::move(states), std::move(mask));
    }

    py::tuple sample_feature_mask_at_t_depth(int depth_t, int n, int seed) const {
        log_call("sample_feature_mask_at_t_depth", "depth=" + std::to_string(depth_t) + " n=" + std::to_string(n) + " seed=" + std::to_string(seed));
        if (n < 0) throw std::runtime_error("n must be nonnegative");

        py::array_t<float> features(std::vector<py::ssize_t>{
            static_cast<py::ssize_t>(n),
            static_cast<py::ssize_t>(kNumMatrixFeatures),
        });
        py::array_t<uint8_t> mask(std::vector<py::ssize_t>{static_cast<py::ssize_t>(n), static_cast<py::ssize_t>(kNumDoubleT)});
        if (n == 0) return py::make_tuple(std::move(features), std::move(mask));

        float* feature_ptr = features.mutable_data();
        uint8_t* mask_ptr = mask.mutable_data();
        fill_sampled_features_states_and_masks(
            depth_t,
            n,
            seed,
            feature_ptr,
            mask_ptr,
            [](std::size_t, const SO6&, std::size_t) {}
        );

        return py::make_tuple(std::move(features), std::move(mask));
    }

    py::array_t<float> apply_tt_features_batch(StateArray3 states, AlphaArray2 alpha_ids) const {
        log_call("apply_tt_features_batch", shape_string(states) + " alphas=" + shape_string(alpha_ids));
        check_states_shape(states, "apply_tt_features_batch");
        if (alpha_ids.ndim() != 2 || alpha_ids.shape(0) != states.shape(0)) {
            throw std::runtime_error("apply_tt_features_batch expects alpha_ids of shape (N,K)");
        }

        const std::size_t n = static_cast<std::size_t>(states.shape(0));
        const std::size_t k = static_cast<std::size_t>(alpha_ids.shape(1));
        py::array_t<float> next_features(std::vector<py::ssize_t>{
            static_cast<py::ssize_t>(n),
            static_cast<py::ssize_t>(k),
            static_cast<py::ssize_t>(kNumMatrixFeatures),
        });
        const word_t* in_ptr = states.data();
        const int64_t* alpha_ptr = alpha_ids.data();
        float* next_feat_ptr = next_features.mutable_data();

        auto work_one = [&](std::size_t i) {
            SO6 source = unpack_state(in_ptr + i * kNumMatrixEntries);
            const int64_t* alpha_row = alpha_ptr + i * k;
            float* next_feat_row = next_feat_ptr + i * k * static_cast<std::size_t>(kNumMatrixFeatures);
            for (std::size_t j = 0; j < k; ++j) {
                SO6 next_out = apply_tt(source, static_cast<int>(alpha_row[j]));
                write_feature_ptr(next_feat_row + j * static_cast<std::size_t>(kNumMatrixFeatures), next_out);
            }
        };

        const std::size_t total_transitions = n * std::max<std::size_t>(k, std::size_t(1));
        if (use_serial_path(total_transitions)) {
            for (std::size_t i = 0; i < n; ++i) {
                work_one(i);
            }
        } else {
            parallel_for_items(n, work_one);
        }
        return next_features;
    }

private:
    void log(const std::string& msg) const {
        if (!verbose_build_ && !debug_ && !log_calls_) return;
        std::cerr << "[readonly_lut] " << msg << " | rss=" << format_bytes(static_cast<double>(rss_bytes())) << std::endl;
    }

    void log_call(const std::string& name, const std::string& detail) const {
        if (log_calls_ || debug_) {
            log("[" + name + "] " + detail);
        }
    }

    std::string cached_depths_string() const {
        std::ostringstream oss;
        oss << "[";
        for (std::size_t i = 0; i < cached_depths_.size(); ++i) {
            if (i) oss << ",";
            oss << cached_depths_[i];
        }
        oss << "]";
        return oss.str();
    }

    template <typename Arr>
    static std::string shape_string(const Arr& arr) {
        std::ostringstream oss;
        oss << "(";
        for (py::ssize_t i = 0; i < arr.ndim(); ++i) {
            if (i) oss << ",";
            oss << arr.shape(i);
        }
        oss << ")";
        return oss.str();
    }

    static void check_states_shape(const StateArray3& states, const std::string& fn) {
        if (states.ndim() != 3 || states.shape(1) != kMatrixSide || states.shape(2) != kMatrixSide) {
            throw std::runtime_error(fn + " expects shape (N,6,6)");
        }
    }

    bool use_serial_path(std::size_t n) const {
        return debug_ || log_calls_ || n < 32;
    }

    template <typename Fn>
    void parallel_for_items(std::size_t n, Fn&& work_one) const {
        py::gil_scoped_release release;
        tbb::parallel_for(tbb::blocked_range<std::size_t>(0, n),
                          [&](const tbb::blocked_range<std::size_t>& range) {
            for (std::size_t i = range.begin(); i != range.end(); ++i) {
                work_one(i);
            }
        });
    }

    template <typename Fn>
    void for_each_item(std::size_t n, Fn&& work_one) const {
        if (use_serial_path(n)) {
            for (std::size_t i = 0; i < n; ++i) {
                work_one(i);
            }
        } else {
            parallel_for_items(n, std::forward<Fn>(work_one));
        }
    }

    void fill_packed_reducing_mask(const SO6& source, const finalized_set& target_layer, PackedMask* out) const {
        clear_packed_mask(out);
        const auto& alpha = tt_alphabet();
        for (int a = 0; a < kNumDoubleT; ++a) {
            SO6 next = apply_tt_pair(source, alpha[static_cast<std::size_t>(a)]);
            if (target_layer.find(next) != target_layer.end()) {
                set_packed_mask_bit(out, a);
            }
        }
    }

    template <typename ExtraWriter>
    void fill_sampled_features_states_and_masks(int depth_t,
                                                int n,
                                                int seed,
                                                float* feature_ptr,
                                                uint8_t* mask_ptr,
                                                ExtraWriter&& write_extra) const {
        const CachedLayer& layer = get_cached_layer_for_sampling(depth_t, n);
        const auto picks = sample_indices(layer.states.size(), n, seed);
        populate_missing_masks(layer, picks);

        auto work_one = [&](std::size_t i) {
            const std::size_t picked_index = picks[i];
            const SO6& source = *layer.states[picked_index];
            write_feature_ptr(feature_ptr + i * static_cast<std::size_t>(kNumMatrixFeatures), source);
            write_extra(i, source, picked_index);
            unpack_cached_mask_row(layer, picked_index, mask_ptr + i * static_cast<std::size_t>(kNumDoubleT));
        };
        for_each_item(static_cast<std::size_t>(n), work_one);
    }

    void unpack_cached_mask_row(const CachedLayer& layer, std::size_t picked_index, uint8_t* row) const {
        std::lock_guard<std::mutex> lock(*layer.reducing_masks_mutex);
        auto it = layer.reducing_masks.find(picked_index);
        if (it == layer.reducing_masks.end()) {
            throw std::runtime_error("requested reducing mask was not populated");
        }
        unpack_packed_mask_row(it->second, row);
    }

    void populate_missing_masks(const CachedLayer& layer, const std::vector<std::size_t>& picks) const {
        if (layer.target_layer == nullptr || picks.empty()) {
            return;
        }

        std::vector<std::size_t> unique_picks = picks;
        std::sort(unique_picks.begin(), unique_picks.end());
        unique_picks.erase(std::unique(unique_picks.begin(), unique_picks.end()), unique_picks.end());

        std::vector<std::size_t> missing;
        {
            std::lock_guard<std::mutex> lock(*layer.reducing_masks_mutex);
            missing.reserve(unique_picks.size());
            for (const std::size_t picked_index : unique_picks) {
                if (layer.reducing_masks.find(picked_index) == layer.reducing_masks.end()) {
                    missing.push_back(picked_index);
                }
            }
        }
        if (missing.empty()) {
            return;
        }

        std::vector<PackedMask> computed(missing.size());
        auto work_one = [&](std::size_t i) {
            const std::size_t picked_index = missing[i];
            const SO6& source = *layer.states[picked_index];
            fill_packed_reducing_mask(source, *layer.target_layer, &computed[i]);
        };
        for_each_item(missing.size(), work_one);

        std::lock_guard<std::mutex> lock(*layer.reducing_masks_mutex);
        for (std::size_t i = 0; i < missing.size(); ++i) {
            layer.reducing_masks.emplace(missing[i], computed[i]);
        }
    }

    const CachedLayer& get_cached_layer_for_sampling(int depth_t, int n) const {
        check_depth(depth_t);
        const CachedLayer& layer = cached_layers_[static_cast<std::size_t>(depth_t)];
        if (!layer.available) {
            throw std::runtime_error("requested sample depth is not cached; rebuild with cached_depths including this depth");
        }
        if (layer.states.empty() && n > 0) {
            throw std::runtime_error("requested nonempty sample from an empty cached layer");
        }
        return layer;
    }

    void check_depth(int depth_t) const {
        if (depth_t < 0 || depth_t >= static_cast<int>(layer_refs_.size())) {
            throw std::runtime_error("requested LUT depth outside the built range");
        }
    }

    const finalized_set& get_layer_ref(int depth_t) const {
        check_depth(depth_t);
        return *layer_refs_[static_cast<std::size_t>(depth_t)];
    }

    void init_layer_refs() {
        layer_refs_.clear();
        layer_refs_.reserve(lut_.size());
        for (auto it = lut_.layers_begin(); it != lut_.layers_end(); ++it) {
            layer_refs_.push_back(&(*it));
        }
    }

    void cache_requested_layers() {
        cached_layers_.clear();
        cached_layers_.resize(layer_refs_.size());

        for (const int d : cached_depths_) {
            const auto& layer = get_layer_ref(d);
            CachedLayer& cached = cached_layers_[static_cast<std::size_t>(d)];
            cached.available = true;
            cached.states.reserve(layer.size());

            const auto start = std::chrono::steady_clock::now();
            for (const auto& s : layer) {
                cached.states.push_back(&s);
            }
            const auto after_state_index = std::chrono::steady_clock::now();

            if (d >= 2) {
                cached.target_layer = &get_layer_ref(d - 2);
            }
            log("[cache_layers] depth=" + std::to_string(d)
                + " indexed " + std::to_string(cached.states.size()) + " states in "
                + format_seconds(std::chrono::duration<double>(after_state_index - start).count())
                + " | reducing masks deferred");
        }
    }

    int max_t_depth_;
    std::string progress_mode_;
    bool verbose_build_;
    bool debug_;
    bool log_calls_;
    std::size_t resolved_threads_{1};
    double build_seconds_{0.0};
    std::vector<int> cached_depths_;
    LUT lut_;
    std::vector<const finalized_set*> layer_refs_;
    std::vector<CachedLayer> cached_layers_;
    std::unique_ptr<tbb::global_control> thread_control_;
};

} // namespace

PYBIND11_MODULE(readonly_lut, m) {
    m.doc() = "Read-only pybind bridge from Exact-Synthesis LUTs to Python with cached masks.";

    py::class_<ReadOnlyLUT>(m, "ReadOnlyLUT")
        .def(
            py::init<int, int, const std::string&, bool, bool, bool, std::vector<int>>(),
            py::arg("max_t_depth") = 12,
            py::arg("threads") = 0,
            py::arg("progress_mode") = "plain",
            py::arg("verbose_build") = true,
            py::arg("debug") = false,
            py::arg("log_calls") = false,
            py::arg("cached_depths") = std::vector<int>{}
        )
        .def("layer_size", &ReadOnlyLUT::layer_size)
        .def(
            "sample_labeled_features_at_t_depth",
            &ReadOnlyLUT::sample_labeled_features_at_t_depth,
            py::arg("depth_t"),
            py::arg("n"),
            py::arg("seed")
        )
        .def(
            "sample_feature_mask_at_t_depth",
            &ReadOnlyLUT::sample_feature_mask_at_t_depth,
            py::arg("depth_t"),
            py::arg("n"),
            py::arg("seed")
        )
        .def(
            "apply_tt_features_batch",
            &ReadOnlyLUT::apply_tt_features_batch,
            py::arg("states"),
            py::arg("alpha_ids")
        );
}
