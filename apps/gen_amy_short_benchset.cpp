// apps/gen_amy_short_benchset.cpp
//
// Generate short 2-qubit circuits in MITMS "searches" format (2 lines per circuit),
// and compute the SO(6) image using the exact generator matrices from the paper.
//
// Outputs OUTDIR:
//   searches              (MITMS input)
//   labels.txt
//   targets/<label>.mat   (SO6 Mathematica string for --target=MAT)
//   meta.csv              (tdepth,tcount,depth)

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "so6/SO6.hpp"

namespace fs = std::filesystem;

// Dyadic constants
static inline DyadicSqrt2 Z() { return DyadicSqrt2(0,0,0); }
static inline DyadicSqrt2 One() { return DyadicSqrt2(1,0,0); }
static inline DyadicSqrt2 NegOne() { return DyadicSqrt2(-1,0,0); }
static inline DyadicSqrt2 InvSqrt2() { return DyadicSqrt2(1,0,1); }
static inline DyadicSqrt2 NegInvSqrt2() { return DyadicSqrt2(-1,0,1); }

static SO6 from_rows(const std::array<std::array<DyadicSqrt2,6>,6>& M) {
    SO6 s;
    for (uint8_t r=0;r<6;++r) for (uint8_t c=0;c<6;++c) s.set_element(r,c,M[r][c]);
    return s;
}

// SO(6) images from your manuscript block
static const SO6& H0() { static const SO6 m = from_rows({{
    {{Z(),Z(),One(),Z(),Z(),Z()}},
    {{Z(),NegOne(),Z(),Z(),Z(),Z()}},
    {{One(),Z(),Z(),Z(),Z(),Z()}},
    {{Z(),Z(),Z(),One(),Z(),Z()}},
    {{Z(),Z(),Z(),Z(),One(),Z()}},
    {{Z(),Z(),Z(),Z(),Z(),One()}}
}}); return m; }

static const SO6& S0() { static const SO6 m = from_rows({{
    {{Z(),NegOne(),Z(),Z(),Z(),Z()}},
    {{One(),Z(),Z(),Z(),Z(),Z()}},
    {{Z(),Z(),One(),Z(),Z(),Z()}},
    {{Z(),Z(),Z(),One(),Z(),Z()}},
    {{Z(),Z(),Z(),Z(),One(),Z()}},
    {{Z(),Z(),Z(),Z(),Z(),One()}}
}}); return m; }

static const SO6& T0() { static const SO6 m = from_rows({{
    {{InvSqrt2(),NegInvSqrt2(),Z(),Z(),Z(),Z()}},
    {{InvSqrt2(),InvSqrt2(), Z(),Z(),Z(),Z()}},
    {{Z(),Z(),One(),Z(),Z(),Z()}},
    {{Z(),Z(),Z(),One(),Z(),Z()}},
    {{Z(),Z(),Z(),Z(),One(),Z()}},
    {{Z(),Z(),Z(),Z(),Z(),One()}}
}}); return m; }

static const SO6& H1() { static const SO6 m = from_rows({{
    {{One(),Z(),Z(),Z(),Z(),Z()}},
    {{Z(),One(),Z(),Z(),Z(),Z()}},
    {{Z(),Z(),One(),Z(),Z(),Z()}},
    {{Z(),Z(),Z(),Z(),Z(),One()}},
    {{Z(),Z(),Z(),Z(),NegOne(),Z()}},
    {{Z(),Z(),Z(),One(),Z(),Z()}}
}}); return m; }

static const SO6& S1() { static const SO6 m = from_rows({{
    {{One(),Z(),Z(),Z(),Z(),Z()}},
    {{Z(),One(),Z(),Z(),Z(),Z()}},
    {{Z(),Z(),One(),Z(),Z(),Z()}},
    {{Z(),Z(),Z(),Z(),NegOne(),Z()}},
    {{Z(),Z(),Z(),One(),Z(),Z()}},
    {{Z(),Z(),Z(),Z(),Z(),One()}}
}}); return m; }

static const SO6& T1() { static const SO6 m = from_rows({{
    {{One(),Z(),Z(),Z(),Z(),Z()}},
    {{Z(),One(),Z(),Z(),Z(),Z()}},
    {{Z(),Z(),One(),Z(),Z(),Z()}},
    {{Z(),Z(),Z(),InvSqrt2(),NegInvSqrt2(),Z()}},
    {{Z(),Z(),Z(),InvSqrt2(),InvSqrt2(), Z()}},
    {{Z(),Z(),Z(),Z(),Z(),One()}}
}}); return m; }

static SO6 gate_on_q0(const std::string& g) {
    if (g == "I") return SO6::identity();
    if (g == "H") return H0();
    if (g == "S") return S0();
    if (g == "T") return T0();
    throw std::runtime_error("unsupported q0 gate: " + g);
}
static SO6 gate_on_q1(const std::string& g) {
    if (g == "I") return SO6::identity();
    if (g == "H") return H1();
    if (g == "S") return S1();
    if (g == "T") return T1();
    throw std::runtime_error("unsupported q1 gate: " + g);
}

static SO6 stage_matrix(const std::string& g0, const std::string& g1) {
    // Tensor product stage: apply both in the same time step
    // SO(6) representation: multiply the corresponding SO(6) images.
    // They commute because they act on disjoint planes, but keep deterministic order.
    return gate_on_q1(g1) * gate_on_q0(g0);
}

static SO6 so6_of_rows(const std::vector<std::string>& row0,
                       const std::vector<std::string>& row1) {
    if (row0.size() != row1.size()) throw std::runtime_error("row lengths differ");
    SO6 cur = SO6::identity();
    for (size_t t = 0; t < row0.size(); ++t) {
        cur = stage_matrix(row0[t], row1[t]) * cur;
    }
    return cur;
}

static std::string join_tokens(const std::vector<std::string>& toks) {
    std::ostringstream ss;
    for (size_t i=0;i<toks.size();++i) {
        if (i) ss << " ";
        ss << toks[i];
    }
    return ss.str();
}

int main(int argc, char** argv) {
    fs::path outdir = (argc > 1) ? fs::path(argv[1]) : fs::path("benchset_short");
    std::uint64_t seed = (argc > 2) ? std::strtoull(argv[2], nullptr, 10) : 1337ull;
    int n_instances = (argc > 3) ? std::atoi(argv[3]) : 30;
    int max_tdepth = (argc > 4) ? std::atoi(argv[4]) : 4;

    if (n_instances < 1) n_instances = 1;
    if (max_tdepth < 1) max_tdepth = 1;

    fs::create_directories(outdir);
    fs::create_directories(outdir / "targets");

    std::ofstream searches(outdir / "searches");
    std::ofstream labels(outdir / "labels.txt");
    std::ofstream meta(outdir / "meta.csv");
    meta << "label,tdepth,tcount,depth\n";

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> pick_tdepth(1, max_tdepth);
    std::uniform_int_distribution<int> pick_side(0, 1);    // which qubit gets T at a T-layer
    std::uniform_int_distribution<int> pick_gap(0, 2);     // number of Clifford layers between T-layers
    std::uniform_int_distribution<int> pick_cliff(0, 3);   // H0,H1,S0,S1
    std::bernoulli_distribution add_prefix(0.5), add_suffix(0.5);

    auto rand_cliff_layer = [&]() -> std::pair<std::string,std::string> {
        switch (pick_cliff(rng)) {
            case 0: return {"H","I"};
            case 1: return {"I","H"};
            case 2: return {"S","I"};
            default: return {"I","S"};
        }
    };
    auto rand_T_layer = [&]() -> std::pair<std::string,std::string> {
        return (pick_side(rng) == 0) ? std::pair{"T","I"} : std::pair{"I","T"};
    };

    for (int i = 0; i < n_instances; ++i) {
        int tdepth = pick_tdepth(rng);
        int tcount = tdepth; // one T per T-layer

        std::ostringstream lab;
        lab << "xs_" << std::setw(3) << std::setfill('0') << i
            << "_td" << tdepth;
        std::string label = lab.str();

        std::vector<std::string> row0, row1;
        auto push_layer = [&](const std::pair<std::string,std::string>& st){
            row0.push_back(st.first);
            row1.push_back(st.second);
        };

        if (add_prefix(rng)) {
            int k = pick_gap(rng);
            for (int j=0;j<k;++j) push_layer(rand_cliff_layer());
        }

        for (int t=0;t<tdepth;++t) {
            push_layer(rand_T_layer());
            int gap = pick_gap(rng);
            for (int j=0;j<gap;++j) push_layer(rand_cliff_layer());
        }

        if (add_suffix(rng)) {
            int k = pick_gap(rng);
            for (int j=0;j<k;++j) push_layer(rand_cliff_layer());
        }

        const int depth = static_cast<int>(row0.size());

        SO6 target = so6_of_rows(row0, row1);

        // MITMS searches block (2 lines of tokens)
        searches << label << " 2\n";
        searches << join_tokens(row0) << "\n";
        searches << join_tokens(row1) << "\n";

        labels << label << "\n";
        meta << label << "," << tdepth << "," << tcount << "," << depth << "\n";

        // SO6 target MAT string
        std::ofstream tf(outdir / "targets" / (label + ".mat"));
        target.print_mathematica(tf);
        tf << "\n";
    }

    std::cout << "Wrote " << n_instances << " instances to " << outdir << "\n";
    return 0;
}

