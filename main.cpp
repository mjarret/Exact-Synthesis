/**
 * T Operator Product Generation Main File
 * @file main.cpp
 * @author Michael Jarret
 * @author Andrew Glaudell
 * @author Sam Mendelson
 * @author Mingzhen Tian
 * @version 11/22/24
 */

#include <chrono>
#include <fstream>
#include <omp.h>
#include <tbb/concurrent_unordered_set.h>
#include <set>
#include "Globals.hpp"
#include "utils.hpp"

using namespace std;

/**
 * @brief Inserts all permutations of a given pattern into a set.
 * 
 * This function generates all permutations of the input pattern and inserts them into a set. 
 * It also considers modifications of each permutation by toggling the rows, and includes the transposed version of the pattern.
 * 
 * @param p The pattern to permute and modify.
 */
static tbb::concurrent_unordered_set<pattern> permutation_set(pattern &pat)
{   
    // Insert the original pattern into the set
    tbb::concurrent_unordered_set<pattern> perms;
    tbb::concurrent_set<pattern> inits;
    inits.insert(pat);
    // inits.insert(pat.transpose());

    for(pattern p : inits) {
        perms.insert(p); 
        // Initialize an array to represent the rows of the pattern
        int row[6] = {0, 1, 2, 3, 4, 5};

        // Generate all permutations of the pattern
        while (std::next_permutation(row, row + 6))
        {
            // Create a new pattern based on the current permutation
            pattern perm_of_orig;
            for (int c = 0; c < 6; c++)
            {
                for (int r = 0; r < 6; r++)
                    perm_of_orig.set(r,c, p.get(row[r],c));
            }
            perms.insert(perm_of_orig);
            // Iterate over all possible combinations of row modifications
            for(unsigned int counter = 0; counter < (1 << 6); counter++) {
                pattern mod_of_perm = perm_of_orig; // Start with a copy of the original permutation
                for(int j = 0; j < 6; j++) {
                    if((counter >> j) & 1) {
                        // Modify the j-th row if the j-th bit of 'counter' is set
                        mod_of_perm.mod_row(j);
                    }
                }
                perms.insert(mod_of_perm);
            }
        }
    }
    return perms;
}

/**
 * @brief Inserts all permutations of a given pattern into a set.
 * 
 * This function generates all permutations of the input pattern and inserts them into a set. 
 * It also considers modifications of each permutation by toggling the rows, and includes the transposed version of the pattern.
 * 
 * @param p The pattern to permute and modify.
 */
static void insert_all_permutations(pattern &p)
{   
    pattern_set.insert(p);
    // tbb::concurrent_set<pattern> perms_of_p = permutation_set(p); 
    // pattern_set.insert(perms_of_p.begin(),perms_of_p.end());
}

static void erase_all_permutations(pattern &p)
{
    if(!pattern_set.contains(p)) return;
    tbb::concurrent_unordered_set<pattern> perms_of_p = permutation_set(p);

    // Collect keys to erase
    std::set<pattern> keys_to_erase;
    for (const auto& entry : perms_of_p) {
        keys_to_erase.insert(entry); // Collect keys
    }

    // Erase from pattern_set
    for (const auto& key : keys_to_erase) {
        if (pattern_set.contains(key)) {
            pattern_set.unsafe_erase(key);
        }
    }
}

/// @brief Reads binary patterns from a file and processes them.
///        Each line in the file is expected to be a binary string representing a pattern.
///        This function converts each line into a pattern object, orders it lexicographically,
///        inserts all its permutations into a set, and then handles the identity pattern.
static void read_pattern_file(std::string pattern_file_path)
{
    if(pattern_file_path.empty()) return;

    std::cout << "[Read] Reading patterns from " << pattern_file << std::endl;
    std::ifstream patternFile(pattern_file_path); // More descriptive variable name

    if (!patternFile.is_open())
    {
        std::cerr << "Failed to open pattern file: " << pattern_file_path << std::endl;
        return;
    }

     // Renamed for clarity
    std::string line;
    int k = 0;
    while (std::getline(patternFile, line))
    {
        pattern currentPattern(line); 
        int case_num = currentPattern.case_num();
        if(case_num == 0) continue;
        currentPattern.id = line;
        insert_all_permutations(currentPattern);
    }
    patternFile.close(); // Close file after processing

    // Handle special case of the identity pattern
    pattern identityPattern = pattern::identity();
    pattern_set.unsafe_erase(identityPattern);
    pattern_set.unsafe_erase(identityPattern.pattern_mod());
    std::cout << "[Finished] Loaded " << pattern_set.size() << " non-identity patterns." << std::endl;
}

static std::chrono::_V2::high_resolution_clock::time_point now()
{
    return chrono::_V2::high_resolution_clock::now();
}

/**
 * @brief Erases the pattern of an SO6 from pattern_set
 * @param s the SO6 to be erased
 */
static bool erase_pattern(SO6 &s) {
    pattern pat = s.to_pattern();
    bool ret = false;
    if (pattern_set.contains(pat)) {
        erase_all_permutations(pat);
        ret = true;
    }
    return ret;
}

/**
 * @brief Erases the pattern of an SO6 from pattern_set
 * @param s the SO6 to be erased
 */
static void record_pattern(SO6 &s, std::ofstream& of) {
    omp_set_lock(&omp_lock);
    of << s.circuit_string() << std::endl;
    omp_unset_lock(&omp_lock);
}

/**
 * @brief Erases the pattern of an SO6 from pattern_set
 * @param s the SO6 to be erased
 */
static void erase_and_record_pattern(SO6 &s, std::ofstream& of) {
    if(erase_pattern(s)) record_pattern(s,of);
}

/// @brief Reads dat file and prints string of gates circuit
/// @param file_name 
static void read_dat(std::string file_name) {
    std::string line;
    std::ifstream file(file_name);
    std::ofstream null_stream("/dev/null");
    if (file.is_open()) {
        while (getline(file, line)) {
            SO6 s = SO6::reconstruct_from_circuit_string(line);
            std::cout << "current size: " << pattern_set.size() << "\n";
            erase_pattern(s);
            std::cout << s.circuit_string() << "\n";
        }
    }
}

static std::string time_since(std::chrono::_V2::high_resolution_clock::time_point &s)
{
    chrono::duration<double> duration = now() - s;
    int64_t time = chrono::duration_cast<chrono::milliseconds>(duration).count();
    if (time < 1000)
        return std::to_string(time).append("ms");
    if (time < 60000)
        return std::to_string((float)time / 1000).substr(0, 5).append("s");
    if (time < 3600000)
        return std::to_string((float)time / 60000).substr(0, 5).append("min");
    if (time < 86400000)
        return std::to_string((float)time / 3600000).substr(0, 5).append("hr");
    return std::to_string((float)time / 86400000).substr(0, 5).append("days");
}

/**
 * @brief Method to report the start of a T count iteration
 * @param T the number of the current T count
 */
static void report_begin_T_count(const int T) {
    std::cout << " ||\t[Start] Beginning T=" << T << std::endl;
    tcount_init_time = now();
}

/**
 * @brief Method to report percentage completed
 * @param c current integer countr
 * @param s total size
 */
inline static void report_percent_complete(const uint64_t &c, const uint64_t s)
{
    if ((c & 0x7F) == 0) // if c is divisible by 128
    {
        std::cout << "\033[A\033[A\r ||\t↪ [Progress] Processing .....    "
                  << (100*c/s) << "\%" << "\n ||\t↪ [Patterns] "
                  << pattern_set.size() << " patterns remain." << std::endl;
    }
}

/**
 * @brief Function to report completion
 * @param matrices_found how many matrices were found
 * @param b flag indicating whether to print the number of matrices found
 * @param of output file stream to close
 */
static void finish_io(const uint &matrices_found, const bool b, std::ofstream &of) {
    std::cout << "\033[A\033[A\r ||\t↪ [Progress] Processing .....    100%" << std::endl; 
    std::cout << " ||\t↪ [Patterns] " << pattern_set.size() << " patterns remain." << std::endl;
    if (b) {
        std::cout << " ||\t↪ [Finished] Found " << matrices_found << " new matrices in " << time_since(tcount_init_time) << "\n ||" << std::endl;
    } else {
        std::cout << " ||\t↪ [Finished] Completed in " << time_since(tcount_init_time) << "\n ||" << std::endl;
    }
    of.close();
}

static std::ofstream prepare_T_count_io(const int t, uint8_t &stored_depth_max, uint8_t &target_T_count) {
    int free_multiply_depth = utils::free_multiply_depth(target_T_count,stored_depth_max);
    // Begin reporting for T=1 with specific depth information
    if (t == 1) {
        std::cout << "\n\n[Begin] Generating T=1 through T=" << static_cast<int>(stored_depth_max) << " iteratively, but will only save "
                  << (free_multiply_depth >= 2 ? (free_multiply_depth == 2 ? "T=2" : "2≤T≤" + std::to_string(free_multiply_depth)) + " and " : "")
                  << "T=" << static_cast<int>(stored_depth_max) << " in memory\n ||\n";
    }

    report_begin_T_count(t);
    std::string file_string = "./data/" + to_string(t) + ".dat";
    std::ofstream of;
    of.open(file_string, ios::out | ios::trunc);
    if (!of.is_open()) std::exit(0);
    std::cout << " ||\t↪ [Save] Opening file " << file_string << "\n"
              << (t == stored_depth_max + 1 ? " ||\t↪ [Rep] Left multiplying everything by T₀\n" : 
                  (t > stored_depth_max + 1 ? " ||\t↪ [Rep] Using generating_set[" + std::to_string(t - stored_depth_max - 1) + "]\n" : ""))
              << " ||\t↪ [Progress] Processing .....    0%\n"
              << " ||\t↪ [Patterns] " << pattern_set.size() << " patterns remain." << std::endl;
    return of;
}

/**
 * @brief Store specific cosets T_0{curr} based on the current T count and free multiply depth.
 * This method saves a subset of the SO6 objects to the generating set, which are used in later iterations.
 * 
 * @param curr_T_count The current T count in the main computation loop.
 * @param free_multiply_depth The depth until which free multiplication is performed.
 * @param num_generating_sets The total number of generating sets.
 * @param current The current set of SO6 objects.
 * @param generating_set Reference to an array of vectors of SO6 objects to store the generated sets.
 */
void storeCosets(int curr_T_count, 
                 tbb::concurrent_set<SO6>& current, std::vector<SO6> &generating_set)
{
    int ngs = utils::num_generating_sets(target_T_count,stored_depth_max);
    if (curr_T_count < ngs)
    {
        std::cout << "\033[A\r ||\t↪ [Save] Saving coset T₀{T=" << curr_T_count + 1 << "} as generating_set[" << curr_T_count << "]\n ||" << std::endl;
        generating_set = std::vector<SO6>(current.begin(),current.end());
        generating_set.erase(std::remove_if(generating_set.begin(), generating_set.end(),
                                [](SO6& S) {
                                    return (S.circuit_string().back() == '0');
                                }),
                    generating_set.end());

        for(SO6 &S : generating_set) {
                S = S.left_multiply_by_T(0);
        }
    }
}

/**
 * @brief The main function of the program.
 *
 * This function is the entry point of the program. It initializes the necessary parameters,
 * reads pattern and case files, performs various operations on the data, and outputs the results.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return The exit status of the program.
 */
int main(int argc, char **argv)
{
    auto program_init_time = now();          // Begin timekeeping
    Globals::setParameters(argc, argv);      // Initialize parameters to command line argument
    Globals::configure();                    // Configure the globals to remove inconsistencies
    read_pattern_file(pattern_file);         // Read the pattern file

    tbb::concurrent_set<SO6> prior, current = tbb::concurrent_set<SO6>({root});

    // This stores the generating sets. Note that the initial generating set is just the 15 T matrices and, thus, doesn't need to be stored
    int ngs = utils::num_generating_sets(target_T_count, stored_depth_max);

    std::vector<SO6> generating_set[ngs];    

    std::mutex mtx;
    std::atomic<bool> lock_held{false};

    for (int curr_T_count = 0; curr_T_count < stored_depth_max; ++curr_T_count)
    {
        tbb::concurrent_set<SO6> next;
        std::ofstream of = prepare_T_count_io(curr_T_count+1,stored_depth_max,target_T_count);

        tbb::concurrent_unordered_set<pattern> patterns;

        uint64_t count = 0, interval_size = (15*current.size()) / THREADS;

        int insert_counter = 0;
        #pragma omp parallel num_threads(THREADS)
        {
            int thread_id = omp_get_thread_num();
            #pragma omp for collapse(2) schedule(dynamic) nowait
            for (size_t i = 0; i < current.size(); ++i) for (int T = 0; T < 15; T++)
            {                
                if (thread_id == 0)  report_percent_complete(++count, interval_size);

                auto it = std::next(current.begin(), i);
                const SO6& S = *it;
                SO6 toInsert = S.left_multiply_by_T(T);
                if(prior.find(toInsert) == prior.end()) {       // This used to be done by finding the differences later, but this works better with the concurrent set
                    if(next.insert(toInsert).second) {
                        erase_and_record_pattern(toInsert, of);
                    }
                }
            }
        }

        utils::rotate_and_clear(prior, current, next); // current is now ready for next iteration
        finish_io(current.size(), true, of);
        storeCosets(curr_T_count, current, generating_set[curr_T_count]);
    }
    
    tbb::concurrent_set<SO6>().swap(prior); // Swap to clear
    std::cout << " ||\n[End] Stored T=" << (int)stored_depth_max << " as current to generate T=" << stored_depth_max + 1 << " through T=" << (int)target_T_count << "\n" << std::endl;

    std::vector<SO6> to_compute = utils::convert_to_vector_and_clear(current);

    std::cout << "[Report] Current patterns: " << pattern_set.size() << std::endl;

    std::cout << "[Begin] Beginning brute force multiply.\n ||" << std::endl;
    uint64_t set_size = to_compute.size();
    uint64_t interval_size = std::ceil(set_size / THREADS); // Equally divide among threads, not sure how to balance but each should take about the same time

    for (int curr_T_count = stored_depth_max; curr_T_count < target_T_count; ++curr_T_count)
    {    
        std::ofstream of = prepare_T_count_io(curr_T_count+1,stored_depth_max, target_T_count);

        std::vector<std::ofstream> file_stream(THREADS);

        omp_init_lock(&omp_lock);
        #pragma omp parallel for schedule(static, interval_size) num_threads(THREADS)
        for (uint64_t i = 0; i < set_size; i++)
        {
            int current_thread = omp_get_thread_num();
            const SO6 &S = to_compute.at(i); 
            if (omp_get_thread_num() == 0)
                report_percent_complete(i % interval_size, interval_size);

            if (curr_T_count == stored_depth_max)
            {
                SO6 N = S.left_multiply_by_T(0);
                if(!cases_flag) {
                    erase_and_record_pattern(N, of);
                    continue;
                }
            }

            for (const SO6 &G : generating_set[curr_T_count-stored_depth_max - 1])
            {
                SO6 N = G*S; 
                if(!cases_flag) {
                    erase_and_record_pattern(N, of);
                    continue;
                }
            }
        }
        omp_destroy_lock(&omp_lock);
        finish_io(0, false, of);
        for(auto &stream : file_stream) stream.close();
    }
    std::cout << " ||\n[Finished] Free multiply complete.\n\n[Time] Total time elapsed: " << time_since(program_init_time) << std::endl;
    std::cout << " Even calls: " << counter_even << " Odd calls: " << counter_odd << " Zero calls: " << counter_zero << std::endl;
    return 0;
}
