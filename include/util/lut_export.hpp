/**
 * @file lut_export.hpp
 * @brief Serialize LUT entries to a compact, searchable on-disk format.
 */
#pragma once

#include <cstdint>
#include <filesystem>

#include "ds/LUT.hpp"

namespace lut_export {

struct ExportSummary {
    std::filesystem::path path;
    std::uint64_t records{0};
    std::uint64_t bytes_written{0};
    std::uint64_t bucket_count{0};
};

// Persist all canonicalized SO6 entries in the LUT to disk.
ExportSummary write_lut_database(const LUT& lut, const std::filesystem::path& dir = "data");

} // namespace lut_export
