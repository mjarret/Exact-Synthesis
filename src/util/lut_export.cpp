/**
 * @file lut_export.cpp
 * @brief Export LUT contents into a compact, hash-indexed binary database.
 */

#include "util/lut_export.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {

constexpr std::size_t kHashBuckets = static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1u;

struct PackedSO6Record {
    std::uint16_t hash;
    // Previous format packed 108 raw bytes (36 * 3). We now store Dyadic words (stable across layouts).
    std::array<std::uint64_t, 36> data;
};
static_assert(std::is_trivially_copyable_v<PackedSO6Record>, "PackedSO6Record must be trivially copyable.");

struct DbHeader {
    char magic[8]{'S','O','6','D','B','V','1','\0'};
    std::uint32_t version{1};
    std::uint32_t record_size{static_cast<std::uint32_t>(sizeof(PackedSO6Record))};
    std::uint64_t record_count{0};
    std::uint32_t bucket_count{static_cast<std::uint32_t>(kHashBuckets)};
    std::uint32_t reserved{0};
};
static_assert(sizeof(DbHeader) == 32, "DbHeader size changed; update writer/reader accordingly.");

} // namespace

namespace lut_export {

ExportSummary write_lut_database(const LUT& lut, const std::filesystem::path& dir) {
    const std::filesystem::path data_dir = dir.empty() ? std::filesystem::path("data") : dir;
    std::filesystem::create_directories(data_dir);
    const std::filesystem::path out_path = data_dir / "so6_lut.db";

    std::vector<std::uint64_t> bucket_counts(kHashBuckets, 0);
    std::uint64_t total_records = 0;

    // Pass 1: count records per hash bucket.
    for (const auto& layer : lut.layers()) {
        for (const auto& s : layer) {
            const std::uint16_t h = s.primary_hash(); // does not canonicalize; only (re)computes cached hash
            ++bucket_counts[h];
            ++total_records;
        }
    }

    std::vector<std::uint64_t> bucket_offsets(kHashBuckets + 1, 0);
    std::uint64_t running = 0;
    for (std::size_t i = 0; i < kHashBuckets; ++i) {
        bucket_offsets[i] = running;
        running += bucket_counts[i];
    }
    bucket_offsets[kHashBuckets] = running;

    std::vector<PackedSO6Record> records(total_records);
    std::vector<std::uint64_t> cursors = bucket_offsets;

    // Pass 2: materialize and place each record into its bucket-reserved slot.
    for (const auto& layer : lut.layers()) {
        for (const auto& s : layer) {
            PackedSO6Record rec{};
            rec.hash = s.primary_hash(); // cached hash only; preserves LUT structure
            for (std::size_t i = 0; i < rec.data.size(); ++i) {
                rec.data[i] = s.arr_[i].data;
            }

            const auto idx = cursors[rec.hash]++;
            records[idx] = rec;
        }
    }

    DbHeader header{};
    header.record_count = total_records;

    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to open LUT database for writing at " + out_path.string());
    }

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(bucket_offsets.data()), bucket_offsets.size() * sizeof(std::uint64_t));
    out.write(reinterpret_cast<const char*>(records.data()), records.size() * sizeof(PackedSO6Record));

    if (!out) {
        throw std::runtime_error("Failed while writing LUT database to " + out_path.string());
    }

    const std::uint64_t bytes_written = sizeof(header)
        + bucket_offsets.size() * sizeof(std::uint64_t)
        + records.size() * sizeof(PackedSO6Record);

    return ExportSummary{
        .path = out_path,
        .records = total_records,
        .bytes_written = bytes_written,
        .bucket_count = kHashBuckets
    };
}

} // namespace lut_export
