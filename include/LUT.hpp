/**
 * @file LUT.hpp
 * @brief Layered lookup table (LUT) of SO6 sets by T-depth.
 */
#ifndef LUT_HPP
#define LUT_HPP

#include <vector>
#include <optional>
#include <tbb/concurrent_unordered_set.h>
#include <lz4.h>
#include <fstream>
#include <variant>
#include <unordered_set>
#include <robin_hood.h>
#include "Z2.hpp"
#include "SO6.hpp"
#include "io_utils.hpp"
#include "signatures.hpp"

using working_set = tbb::concurrent_unordered_set<SO6>;
using finalized_set = robin_hood::unordered_flat_set<SO6>;
static const finalized_set empty_set;

#ifdef _WIN32
#include <windows.h>
size_t getAvailableMemory() {
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return statex.ullAvailPhys;
    }
    return 0;
}
#else
#include <sys/sysinfo.h>
size_t getAvailableMemory() {
    struct sysinfo memInfo;
    if (sysinfo(&memInfo) == 0) {
        return memInfo.freeram * memInfo.mem_unit;
    }
    return 0;
}
#endif

/**
 * @brief Accumulates finalized SO6 sets layer-by-layer with memory-aware finalization.
 */
class LUT {

public:

    std::string prefix = "";

    LUT(const SO6& root = SO6::identity(), std::string prefix = "") : prefix(prefix) {
        lookupTable.push_back(finalized_set{});
        lookupTable.back().insert(root);
    };

    void finalize_current_set(indicators::ProgressBar* finalize_bar = nullptr) {
        size_t availableMemory = getAvailableMemory();
        size_t elementSize = sizeof(SO6);
        size_t maxElements = availableMemory / elementSize;
        if(maxElements == 0)
            throw std::runtime_error("Insufficient memory to store the lookup table");

        finalized_set robin_set;
        lookupTable.push_back(std::move(robin_set));
        // Tune memory: higher load factor and pre-reserve expected size
        // lookupTable.back().max_load_factor(0.9f);
        // lookupTable.back().reserve(finalSet.size());
        // lookupTable.back().max_load_factor(2.0f);
        lookupTable.back().reserve(finalSet.size());

        size_t count = 0;
        for (const auto& v : finalSet) {
            lookupTable.back().insert(v);
            if (finalize_bar) finalize_bar->set_progress(++count);
        }
        working_set().swap(finalSet);

        // Optional hash profiling sweep (set EXACT_SYNTH_HASH_SWEEP=1 in env)
        const char* sweep = std::getenv("EXACT_SYNTH_HASH_SWEEP");
        if (sweep && std::string(sweep) == "1") {
            const size_t N = lookupTable.back().size();
            if (N == 0) return;
            size_t maxSample = 50000; // default sample cap
            if (const char* s = std::getenv("EXACT_SYNTH_HASH_SAMPLE")) {
                try { maxSample = static_cast<size_t>(std::stoull(s)); } catch (...) {}
            }
            if (maxSample == 0) maxSample = 1;
            const size_t stride = std::max<size_t>(1, N / maxSample);
            // Open log file if provided; default to hash_sweep.log
            std::string log_path = "hash_sweep.log";
            if (const char* lp = std::getenv("EXACT_SYNTH_HASH_LOG")) log_path = lp;
            std::ofstream logf(log_path, std::ios::app);
            bool log_ok = static_cast<bool>(logf);
            constexpr int variants[] = {0,1,2,3,4,5,6};
            for (int var : variants) {
                std::unordered_map<size_t, size_t> counts;
                counts.reserve(std::min(N, maxSample) * 2);
                size_t max_dup = 0;
                size_t sampled = 0;
                size_t idx = 0;
                for (const auto &s : lookupTable.back()) {
                    if ((idx++ % stride) != 0) continue; // downsample
                    size_t row_sig=0,col_sig=0;
                    SO6::signature_row_col_variant(s, var, row_sig, col_sig);
                    size_t h = row_sig ^ (col_sig + 0x9e3779b97f4a7c15ULL + (row_sig<<6) + (row_sig>>2));
                    auto &c = counts[h];
                    max_dup = std::max(max_dup, ++c);
                    if (++sampled >= maxSample) break;
                }
                size_t distinct = counts.size();
                size_t collisions = sampled > distinct ? sampled - distinct : 0;
                std::ostringstream oss;
                oss << "[HashSweep] T=" << lookupTable.size() << " var=" << var
                    << " N=" << N << " sample=" << sampled << " distinct=" << distinct
                    << " collisions=" << collisions << " maxdup=" << max_dup << " stride=" << stride << "\n";
                if (log_ok) logf << oss.str(); else std::cout << oss.str();
            }
            if (log_ok) logf.flush();
        }

        // Optional signature profiling sweep (set EXACT_SYNTH_SIG_SWEEP=1)
        const char* sigsweep = std::getenv("EXACT_SYNTH_SIG_SWEEP");
        if (sigsweep && std::string(sigsweep) == "1") {
            const size_t N = lookupTable.back().size();
            if (N != 0) {
                size_t maxSample = 50000; // default sample cap
                if (const char* s = std::getenv("EXACT_SYNTH_SIG_SAMPLE")) {
                    try { maxSample = static_cast<size_t>(std::stoull(s)); } catch (...) {}
                }
                if (maxSample == 0) maxSample = 1;
                const size_t stride = std::max<size_t>(1, N / maxSample);
                std::string log_path = "sig_sweep.log";
                if (const char* lp = std::getenv("EXACT_SYNTH_SIG_LOG")) log_path = lp;
                std::ofstream logf(log_path, std::ios::app);
                bool log_ok = static_cast<bool>(logf);
                // Try several signature variants
                constexpr int variants[] = {0,1,2,3,4};
                for (int var : variants) {
                    std::unordered_map<size_t, size_t> counts;
                    counts.reserve(std::min(N, maxSample) * 2);
                    size_t max_dup = 0;
                    size_t sampled = 0;
                    size_t idx = 0;
                    for (const auto &s : lookupTable.back()) {
                        if ((idx++ % stride) != 0) continue; // downsample
                        size_t h = signatures::compute(var, s);
                        auto &c = counts[h];
                        max_dup = std::max(max_dup, ++c);
                        if (++sampled >= maxSample) break;
                    }
                    size_t distinct = counts.size();
                    size_t collisions = sampled > distinct ? sampled - distinct : 0;
                    std::ostringstream oss;
                    oss << "[SigSweep] T=" << lookupTable.size() << " var=" << var
                        << " N=" << N << " sample=" << sampled << " distinct=" << distinct
                        << " collisions=" << collisions << " maxdup=" << max_dup << " stride=" << stride << "\n";
                    if (log_ok) logf << oss.str(); else std::cout << oss.str();
                }
                if (log_ok) logf.flush();
            }
        }
    }

    void insert(size_t index, const SO6& value) {
        if (index == lookupTable.size()) {
            finalSet.insert(value);
        }
    }

    bool contains(size_t index, const SO6& value) const {
        if (index < lookupTable.size()) {
            return lookupTable[index].find(value) != lookupTable[index].end();
        } else if (index == lookupTable.size()) {
            return finalSet.find(value) != finalSet.end();
        }
        return false;
    }

    bool contains(const SO6& value) const {
        for (const auto& set : lookupTable) {
            if (set.find(value) != set.end()) return true;
        }
        return finalSet.find(value) != finalSet.end();
    }

    #ifdef EXACT_SYNTH_USE_SO6_FLATSET
    using find_variant = std::variant<const SO6*, working_set::const_iterator>;
    #else
    using find_variant = std::variant<finalized_set::const_iterator, working_set::const_iterator>;
    #endif

    find_variant find(size_t index, const SO6& value) const {
        if (index < lookupTable.size()) {
            return lookupTable[index].find(value);
        } else if (index == lookupTable.size()) {
            return finalSet.find(value);
        }
        return finalSet.end();
    }

    bool contains_in_prior(const SO6& value) const {
        if(size() == 1) {
            return false;
        }
        return lookupTable[lookupTable.size()-2].find(value) != lookupTable[lookupTable.size()-2].end();
    }

    std::string circuit_string(SO6 s, size_t index = -1) const {
        if (index == -1) index = lookupTable.size();
        s = std::visit([](auto&& arg) { return static_cast<SO6>(*arg); }, find(index, s));
        if(s.last_T == 15) return prefix;
        return static_cast<char>('A' + s.last_T) + circuit_string(s.left_multiply_by_T(s.last_T), index--);
    }

    size_t size() const {
        return lookupTable.size()+1; // 1 for final set
    }

    finalized_set& operator[](size_t index) {
        if (index < lookupTable.size()) {
            return lookupTable[index];
        } else {
            throw std::out_of_range("Index out of range");
        }
    }

    finalized_set& at(size_t index) {
        if (index < lookupTable.size()) {
            return lookupTable[index];
        } else {
            throw std::out_of_range("Index out of range");
        }
    }

    void push_back(const working_set& set) {
        finalSet.insert(set.begin(), set.end());
    }

    const finalized_set& current() {
        return lookupTable.back();
    }

    const finalized_set& prior() const {
        if(size() > 2 ) return lookupTable[lookupTable.size()-2];
        return empty_set;
    }

    auto begin() {
        return lookupTable.begin();
    }

    auto end() {
        return lookupTable.end();
    }

    auto operator*() {
        return lookupTable;
    }

    Z2 get_maximum() {
        Z2 r(0,0,0);
        for(auto set : lookupTable) {
            for (auto s : set) {
                for(auto z : s.arr) {
                    if(std::abs(r).int_c < std::abs(z).int_c) {
                        r.int_c = z.int_c;
                    }
                    if(std::abs(r).sqrt2_c < std::abs(z).sqrt2_c) {
                        r.sqrt2_c = z.sqrt2_c;
                    }
                    if(std::abs(r).denom_exp < std::abs(z).denom_exp) {
                        r.denom_exp = z.denom_exp;
                    }
                }
            }
        }
        for (auto s : finalSet) for(auto z : s.arr) {
                if(std::abs(r).int_c < std::abs(z).int_c) r.int_c = z.int_c;
                if(std::abs(r).sqrt2_c < std::abs(z).sqrt2_c) r.sqrt2_c = z.sqrt2_c;
                if(std::abs(r).denom_exp < std::abs(z).denom_exp) r.denom_exp = z.denom_exp;
        }
        return r;
    }

    void write_to_files() {
        constexpr size_t flushInterval = 1000;  // flush every 1000 circuit strings

        for (size_t i = 0; i < lookupTable.size(); ++i) {
            auto of = io_utils::prepare_T_count_io(i + 1);
            std::string buffer;
            buffer.reserve(flushInterval * 21);
    
            size_t count = 0;
            for (const auto& s_initial : lookupTable[i]) {
                SO6 s = s_initial;

                std::string circuit;
                circuit.reserve(i + 1);
                for (size_t j = i + 1; j > 0; --j) {
                    circuit.push_back(static_cast<char>('A' + s.last_T));
                    s = std::visit([](auto&& arg) { return *arg; }, find(j - 1, s.left_multiply_by_T(s.last_T)));
                }
                buffer.append(circuit);
                buffer.push_back(',');
                
                if ((++count) % flushInterval == 0) {
                    if(count == lookupTable[i].size()) {
                        buffer.pop_back();  // Remove the trailing comma.
                    }
                    of << buffer;
                    buffer.clear();
                }
            }
            buffer.pop_back();  // Remove the trailing comma.
            of << buffer;
            of.close();
        }

        // Write the final set
        auto of = io_utils::prepare_T_count_io(lookupTable.size() + 1);
        std::string buffer;
        buffer.reserve(flushInterval * 21);

        size_t count = 0;
        for (const auto& s_initial : finalSet) {
            SO6 s = s_initial;

            std::string circuit;
            circuit.reserve(lookupTable.size() + 1);
            for (size_t j = lookupTable.size() + 1; j > 0; --j) {
                circuit.push_back(static_cast<char>('A' + s.last_T));
                s = std::visit([](auto&& arg) { return *arg; }, find(j - 1, s.left_multiply_by_T(s.last_T)));
            }
            buffer.append(circuit);
            buffer.push_back(',');

            if ((++count) % flushInterval == 0) {
                if(count == finalSet.size()) {
                    buffer.pop_back();  // Remove the trailing comma.
                }
                of << buffer;
                buffer.clear();
            }
        }
        buffer.pop_back();  // Remove the trailing comma.
        of << buffer;
        of.close();
    }

    void save(const std::string& filename) const {
        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("Failed to open file for writing");
        }

        for (const auto& set : lookupTable) {
            size_t setSize = set.size();
            ofs.write(reinterpret_cast<const char*>(&setSize), sizeof(setSize));

            for (const auto& element : set) {
                std::string serialized = element.serialize();
                size_t dataSize = serialized.size();
                ofs.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
                ofs.write(serialized.data(), dataSize);
            }
        }

        // Save the final set
        size_t setSize = finalSet.size();
        ofs.write(reinterpret_cast<const char*>(&setSize), sizeof(setSize));

        for (const auto& element : finalSet) {
            std::string serialized = element.serialize();
            size_t dataSize = serialized.size();
            ofs.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
            ofs.write(serialized.data(), dataSize);
        }
    }

    void load(const std::string& filename) {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs) {
            throw std::runtime_error("Failed to open file for reading");
        }

        lookupTable.clear();
        finalSet.clear();

        while (ifs.peek() != EOF) {
            size_t setSize;
            ifs.read(reinterpret_cast<char*>(&setSize), sizeof(setSize));

            finalized_set set;
            for (size_t i = 0; i < setSize; ++i) {
                size_t dataSize;
                ifs.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

                std::string serialized(dataSize, '\0');
                ifs.read(&serialized[0], dataSize);

                SO6 element = SO6::deserialize(serialized);
                set.insert(element);
            }
            lookupTable.push_back(set);
        }

        // Load the final set
        size_t setSize;
        ifs.read(reinterpret_cast<char*>(&setSize), sizeof(setSize));

        for (size_t i = 0; i < setSize; ++i) {
            size_t dataSize;
            ifs.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

            std::string serialized(dataSize, '\0');
            ifs.read(&serialized[0], dataSize);

            SO6 element = SO6::deserialize(serialized);
            finalSet.insert(element);
        }
    }

private:
    std::vector<finalized_set> lookupTable = {};
    working_set finalSet;
};

#endif // LUT_HPP
