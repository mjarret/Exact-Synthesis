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

// SO(6) images from manuscript (exactly your snippet)
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

static const SO6& CZ() { static const SO6 m = from_rows({{
    {{Z(),NegOne(),Z(),Z(),Z(),Z()}},
    {{One(),Z(),Z(),Z(),Z(),Z()}},
    {{Z(),Z(),Z(),Z(),Z(),NegOne()}},
    {{Z(),Z(),Z(),Z(),NegOne(),Z()}},
    {{Z(),Z(),Z(),One(),Z(),Z()}},
    {{Z(),Z(),One(),Z(),Z(),Z()}}
}}); return m; }

struct Stage {
    std::string a, b;   // MITMS tokens (qubit1, qubit2)
    const SO6* M;       // SO(6) image
    bool isT;
};

static Stage st_H0(){ return {"H","I",&H0(),false}; }
static Stage st_S0(){ return {"S","I",&S0(),false}; }
static Stage st_T0(){ return {"T","I",&T0(),true};  }
static Stage st_H1(){ return {"I","H",&H1(),false}; }
static Stage st_S1(){ return {"I","S",&S1(),false}; }
static Stage st_T1(){ return {"I","T",&T1(),true};  }
static Stage st_CZ(){ return {"C(2)","Z",&CZ(),false}; } // symmetric

static SO6 image_of(const std::vector<Stage>& circ) {
    SO6 cur = SO6::identity();
    for (const auto& st : circ) cur = (*st.M) * cur;  // stage order: first line applied first
    return cur;
}

int main(int argc, char** argv){
    fs::path outdir = (argc>1)? fs::path(argv[1]) : fs::path("benchset");
    uint64_t seed   = (argc>2)? std::strtoull(argv[2],nullptr,10) : 1337ULL;
    int kmax        = (argc>3)? std::atoi(argv[3]) : 15;
    int perK        = (argc>4)? std::atoi(argv[4]) : 20;

    fs::create_directories(outdir);
    fs::create_directories(outdir/"targets");

    std::ofstream searches(outdir/"searches");
    std::ofstream labels(outdir/"labels.txt");
    std::ofstream meta(outdir/"meta.csv");
    meta << "label,raw_T,stages\n";

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> cliff(0,4);     // H0,H1,S0,S1,CZ
    std::uniform_int_distribution<int> tside(0,1);      // T0 or T1
    std::uniform_int_distribution<int> gap(0,3);        // Cliffords between Ts
    std::uniform_int_distribution<int> prefix(0,3), suffix(0,3);

    auto rand_cliff = [&](){
        switch(cliff(rng)){
            case 0: return st_H0();
            case 1: return st_H1();
            case 2: return st_S0();
            case 3: return st_S1();
            default: return st_CZ();
        }
    };
    auto rand_T = [&](){ return (tside(rng)==0)? st_T0() : st_T1(); };

    for(int k=1;k<=kmax;++k){
        for(int i=0;i<perK;++i){
            std::ostringstream lab;
            lab << "randT" << std::setw(2) << std::setfill('0') << k
                << "_" << std::setw(4) << std::setfill('0') << i;
            const std::string label = lab.str();

            std::vector<Stage> circ;
            for(int j=0;j<prefix(rng);++j) circ.push_back(rand_cliff());
            for(int t=0;t<k;++t){
                circ.push_back(rand_T());
                for(int j=0;j<gap(rng);++j) circ.push_back(rand_cliff());
            }
            for(int j=0;j<suffix(rng);++j) circ.push_back(rand_cliff());

            SO6 target = image_of(circ);

            // Write MITMS searches entry
            searches << label << " 2\n";
            for(const auto& st : circ) searches << st.a << " " << st.b << "\n";

            labels << label << "\n";
            meta << label << "," << k << "," << circ.size() << "\n";

            // Write target MAT string for our solver
            std::ofstream mt(outdir/"targets"/(label + ".mat"));
            target.print_mathematica(mt);
            mt << "\n";
        }
    }

    std::cout << "Wrote benchset to " << outdir << "\n";
    return 0;
}

