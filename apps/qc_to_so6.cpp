// apps/qc_to_so6.cpp
//
// Read a 2-qubit MITMS-style stage circuit from stdin (no header),
// compute its SO(6) matrix using the manuscript generator images,
// and print a Mathematica matrix string to stdout.

#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "so6/SO6.hpp"

// --- Dyadic constants ---
static inline DyadicSqrt2 Z0() { return DyadicSqrt2(0,0,0); }
static inline DyadicSqrt2 One() { return DyadicSqrt2(1,0,0); }
static inline DyadicSqrt2 NegOne() { return DyadicSqrt2(-1,0,0); }
static inline DyadicSqrt2 InvSqrt2() { return DyadicSqrt2(1,0,1); }
static inline DyadicSqrt2 NegInvSqrt2() { return DyadicSqrt2(-1,0,1); }

static SO6 from_rows(const std::array<std::array<DyadicSqrt2,6>,6>& M) {
    SO6 s;
    for (uint8_t r=0;r<6;++r) for (uint8_t c=0;c<6;++c) s.set_element(r,c,M[r][c]);
    return s;
}

static const SO6& H0() { static const SO6 m = from_rows({{
    {{Z0(),Z0(),One(),Z0(),Z0(),Z0()}},
    {{Z0(),NegOne(),Z0(),Z0(),Z0(),Z0()}},
    {{One(),Z0(),Z0(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),One(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),One(),Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),Z0(),One()}}
}}); return m; }

static const SO6& S0() { static const SO6 m = from_rows({{
    {{Z0(),NegOne(),Z0(),Z0(),Z0(),Z0()}},
    {{One(),Z0(),Z0(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),One(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),One(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),One(),Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),Z0(),One()}}
}}); return m; }

static const SO6& T0() { static const SO6 m = from_rows({{
    {{InvSqrt2(),NegInvSqrt2(),Z0(),Z0(),Z0(),Z0()}},
    {{InvSqrt2(),InvSqrt2(), Z0(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),One(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),One(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),One(),Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),Z0(),One()}}
}}); return m; }

static const SO6& H1() { static const SO6 m = from_rows({{
    {{One(),Z0(),Z0(),Z0(),Z0(),Z0()}},
    {{Z0(),One(),Z0(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),One(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),Z0(),One()}},
    {{Z0(),Z0(),Z0(),Z0(),NegOne(),Z0()}},
    {{Z0(),Z0(),Z0(),One(),Z0(),Z0()}}
}}); return m; }

static const SO6& S1() { static const SO6 m = from_rows({{
    {{One(),Z0(),Z0(),Z0(),Z0(),Z0()}},
    {{Z0(),One(),Z0(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),One(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),NegOne(),Z0()}},
    {{Z0(),Z0(),Z0(),One(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),Z0(),One()}}
}}); return m; }

static const SO6& T1() { static const SO6 m = from_rows({{
    {{One(),Z0(),Z0(),Z0(),Z0(),Z0()}},
    {{Z0(),One(),Z0(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),One(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),InvSqrt2(),NegInvSqrt2(),Z0()}},
    {{Z0(),Z0(),Z0(),InvSqrt2(),InvSqrt2(), Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),Z0(),One()}}
}}); return m; }

static const SO6& CZ() { static const SO6 m = from_rows({{
    {{Z0(),NegOne(),Z0(),Z0(),Z0(),Z0()}},
    {{One(),Z0(),Z0(),Z0(),Z0(),Z0()}},
    {{Z0(),Z0(),Z0(),Z0(),Z0(),NegOne()}},
    {{Z0(),Z0(),Z0(),Z0(),NegOne(),Z0()}},
    {{Z0(),Z0(),Z0(),One(),Z0(),Z0()}},
    {{Z0(),Z0(),One(),Z0(),Z0(),Z0()}}
}}); return m; }

static SO6 transpose(const SO6& A) {
    SO6 out;
    for (uint8_t r=0;r<6;++r) for (uint8_t c=0;c<6;++c)
        out.set_element(r,c, A.get_element(c,r));
    return out;
}

static SO6 gate_on_qubit(int q, const std::string& g) {
    const SO6 I = SO6::identity();
    if (g == "I") return I;

    const SO6& H = (q==0) ? H0() : H1();
    const SO6& S = (q==0) ? S0() : S1();
    const SO6& T = (q==0) ? T0() : T1();

    if (g == "H") return H;
    if (g == "S") return S;
    if (g == "S*") return transpose(S);
    if (g == "T") return T;
    if (g == "T*") return transpose(T);

    // Derived Paulis from H,S (global phase ignored in SO(6))
    if (g == "Z") return S * S;
    if (g == "X") { SO6 Zm = S*S; return H * Zm * H; }
    if (g == "Y") {
        SO6 X = H * (S*S) * H;
        SO6 Sinv = transpose(S);
        return S * X * Sinv;
    }

    throw std::runtime_error("unsupported 1q token: " + g);
}

static SO6 controlled_pauli(int control_q, int target_q, const std::string& G) {
    // Only supports controlled-{X,Y,Z} on 2 qubits by conjugating CZ on the target side.
    // CZ itself is symmetric; we assume control_q != target_q.

    if (G == "Z") return CZ();

    // Choose B such that B Z B^{-1} = G
    // X: B=H
    // Y: B=S H
    if (G != "X" && G != "Y") throw std::runtime_error("unsupported controlled gate: " + G);

    SO6 B = SO6::identity();
    if (G == "X") {
        B = gate_on_qubit(target_q, "H");
    } else { // Y
        // B = S H (apply H then S => matrix S*H)
        SO6 Ht = gate_on_qubit(target_q, "H");
        SO6 St = gate_on_qubit(target_q, "S");
        B = St * Ht;
    }
    SO6 Binv = transpose(B);
    return B * CZ() * Binv;
}

static SO6 stage_matrix(const std::string& a, const std::string& b) {
    // Controls are "C(1)" or "C(2)" in MITMS; treat them as control markers.
    auto isC = [](const std::string& s){ return s.size()>=3 && s.rfind("C(",0)==0; };

    if (!isC(a) && !isC(b)) {
        // tensor product
        SO6 A0 = gate_on_qubit(0, a);
        SO6 A1 = gate_on_qubit(1, b);
        return A1 * A0; // commute, but keep deterministic order
    }

    // Controlled gate: one token is control, the other is target gate
    if (isC(a) && !isC(b)) {
        // control is qubit 0, target is qubit 1
        return controlled_pauli(0, 1, b);
    }
    if (!isC(a) && isC(b)) {
        // control is qubit 1, target is qubit 0
        return controlled_pauli(1, 0, a);
    }

    throw std::runtime_error("unsupported stage with two controls");
}

int main() {
    std::vector<std::pair<std::string,std::string>> stages;

    std::string line;
    while (std::getline(std::cin, line)) {
        // stop on empty line (useful when feeding a block)
        if (line.empty()) break;
        std::istringstream iss(line);
        std::string a,b;
        if (!(iss >> a >> b)) continue;
        stages.push_back({a,b});
    }

    SO6 cur = SO6::identity();
    for (const auto& st : stages) {
        SO6 S = stage_matrix(st.first, st.second);
        cur = S * cur;
    }

    cur.print_mathematica(std::cout);
    std::cout << "\n";
    return 0;
}

