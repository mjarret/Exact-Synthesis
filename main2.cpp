/**
 * T Operator Product Generation Main File
 * @file main.cpp
 * @author Swan Klein
 * @author Connor Mooney
 * @author Michael Jarret
 * @author Andrew Glaudell
 * @author Jacob Weston
 * @author Mingzhen Tian
 * @version 6/12/21
 */

#include <algorithm>
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <fstream>
#include <chrono>
#include <set>
#include <string>
#include <sstream>
#include <functional>
#include <stdint.h>
#include <stdlib.h>
#include <omp.h>
#include "Z2.hpp"
#include "SO6.hpp"


using namespace std;

const int8_t numThreads = 1;
const int8_t tCount = 5;
const Z2 inverse_root2 = Z2::inverse_root2();
const Z2 one = Z2::one();

//Turn this on if you want to read in saved data
const bool tIO = false;
//If tIO true, choose which tCount to begin generating from:
const int8_t genFrom = tCount;

//Saves every saveInterval iterations
const int saveInterval = 50000;


SO6 identity() {
    SO6 I = SO6({-1});
    for(int8_t k =0; k<6; k++) {
        I(k,k) = 1;
    }
    I.lexOrder();
    return I;
}

/**
 * Returns the SO6 object corresponding to T[i+1, j+1]
 * @param i the first index to determine the T matrix
 * @param j the second index to determine the T matrix
 * @param matNum the index of the SO6 object in the base vector
 * @return T[i+1,j+1]
 */
const SO6 tMatrix(int8_t i, int8_t j, int8_t matNum) {
    // Generates the T Matrix T[i+1, j+1]
    SO6 t({matNum});                               
    for(int8_t k = 0; k < 6; k++) t[k][k] = one;          // Initialize to the identity matrix
    t[i][i] = inverse_root2;                              // Change the i,j cycle to appropriate 1/sqrt(2)
    t[j][j] = inverse_root2;
    t[i][j] = inverse_root2;
    if(abs(i-j)!=1) t[i][j].negate();                     // Lexicographic ordering and sign fixing undoes the utility of this, not sure whether to keep it
    t[j][i] = -t[i][j];
    t.fixSign();
    t.lexOrder();
    return(t);
}

set<SO6> fileRead(int8_t tc, vector<SO6> tbase) {
    ifstream tfile;
    tfile.open(("data/T" + to_string(tc) + ".txt").c_str());
    if(!tfile) {
        cout << "File does not exist.\n";
        exit(1);
    }
    set<SO6> tset;
    char hist;
    long i = 0;
    vector<int8_t> tmp;
    SO6 m;
    while(tfile.get(hist)) {
        //Convert hex character to integer
        tmp.push_back((hist >= 'a') ? (hist - 'a' + 10) : (hist - '0'));
        if (++i%tc == 0) {
            m = tbase.at(tmp.at(tmp.size() - 1));
            for(int8_t k = tmp.size()-2; k > -1; k--) {
                m = tbase.at(tmp.at(k))*m;
            }
            tset.insert(m);
            tmp.clear();
        }
    }
    return tset;
}

// This appends next to the T file
void writeResults(int8_t i, int8_t tsCount, long currentCount, set<SO6> &next) {
    auto start = chrono::high_resolution_clock::now();
    string fileName = "data/T" + to_string(i+1) + "index.txt";
    fstream write = fstream(fileName, std::ios_base::out);
    write << +tsCount << ' ' << +currentCount;
    write.close();
    fileName = "data/T" + to_string(i+1) + ".txt";
    write = fstream(fileName, std::ios_base::app);
    for(SO6 n : next) write<<n;
    write.close();
    auto end = chrono::high_resolution_clock::now();
    auto ret = chrono::duration_cast<chrono::milliseconds>(end-start).count();
    cout<<">>>Wrote T-Count "<<(i+1)<<" to 'data/T"<<(i+1)<<".txt' in " << ret << "ms\n";
}

int main(){
    //timing
    auto tbefore = chrono::high_resolution_clock::now();

    set<SO6> prior;
    set<SO6> current({identity()});
    set<SO6> next;
    set<SO6> append;
    ifstream tfile;
    SO6 m;
    int8_t start = 0;
    
    vector<SO6> tsv; //t count 1 matrices
    for(int8_t i = 0; i<15; i++){
        if(i<5)         tsv.push_back(tMatrix(0,i+1,i));
        else if(i<9)    tsv.push_back(tMatrix(1, i-3,i));
        else if(i<12)   tsv.push_back(tMatrix(2, i-6,i));
        else if(i<14)   tsv.push_back(tMatrix(3, i-8,i));
        else            tsv.push_back(tMatrix(4,5,i));
    }

    bool reject[15][15];
    for(int i = 0; i<15; i++) {
        for(int j = 0; j<15; j++) {
            reject[i][j] = (tsv[i]*tsv[j] == identity());
        }
    }

    if(tIO && tCount > 2) {
        prior = fileRead(genFrom-2, tsv);
        current = fileRead(genFrom-1, tsv);
        start = genFrom - 1;
    }
    
    for(int8_t i = start; i<tCount; i++){
        std::cout<<"\nBeginning T-Count "<<(i+1)<<"\n";

        auto start = chrono::high_resolution_clock::now();
        
        // Main loop here
        int8_t tsCount = 0;
        int size;
        long currentCount = 0;
        long save = 0;
        tfile.open(("data/T" + to_string(i + 1) + "index.txt").c_str());
        if (!tIO || !tfile) {
            if (tfile) tfile.close();
            remove(("data/T" + to_string(i + 1) + "index.txt").c_str());
            for(SO6 t : tsv) {
                for(SO6 curr : current) {
                    size = next.size();
                    m = t*curr;
                    next.insert(m);     // New product list for T + 1 stored as next
                    if (size != next.size()) {
                        append.insert(m);
                    }
                    save++;
                    currentCount++;
                    if(save == saveInterval) {
                        for(SO6 p : prior) {
                            next.erase(p);
                            append.erase(p);
                        }
                        writeResults(i, tsCount, currentCount, append);
                        save = 0;
                        append.clear();
                    }
                }
                tsCount++;
                currentCount = 0;
                if(i == tCount -1) break;    // We only need one T matrix at the final T-count
            }
            for(SO6 p : prior) next.erase(p); // Erase T-1
            // Write results out
            writeResults(i, tsv.size(), currentCount, append);
        }
        else {
            next = fileRead(i+1, tsv);
            string str;
            getline(tfile, str);
            stringstream s(str);
            getline(s, str, ' ');
            int8_t tsCount = stoi(str);
            getline(s, str, ' ');
            currentCount = stoi(str);
            tfile.close();

            std::vector<SO6>::iterator titr = tsv.begin();
            std::set<SO6>::iterator citr = current.begin();
            advance(titr, tsCount);
            advance(citr, currentCount);
            SO6 t, curr;
            while (titr != tsv.end()) {
                t = *titr;
                while (citr != current.end()) {
                    curr = *citr;
                    m = t*curr;
                    next.insert(m);
                    if (size != next.size()) {
                        append.insert(m);
                    }
                    save++;
                    citr++;
                    currentCount++;
                    if(save == saveInterval) {
                        for(SO6 p : prior) {
                            append.erase(p);
                        }
                        writeResults(i, tsCount, currentCount, append);
                        save = 0;
                        append.clear();
                    }
                }
                titr++;
                tsCount++;
                currentCount = 0;
                citr = current.begin();
            }
            writeResults(i, tsv.size(), currentCount, append);
        }
        // End main loop
        for(SO6 p : prior) {
            next.erase(p);                   // Erase T - 1
            append.erase(p);
        }
        auto end = chrono::high_resolution_clock::now();
        // Begin reporting
        auto ret = chrono::duration_cast<chrono::milliseconds>(end-start).count();
        std::cout << ">>>Found " << next.size() << " new matrices in " << ret << "ms\n";
        prior.clear();
        prior.swap(current);                                    // T++
        current.swap(next);                                     // T++

        // Write results out
        writeResults(i, tsCount, currentCount, append);
        append.clear();
    }
    chrono::duration<double> timeelapsed = chrono::high_resolution_clock::now() - tbefore;
    std::cout<< "\nTotal time elapsed: "<<chrono::duration_cast<chrono::milliseconds>(timeelapsed).count()<<"ms\n";
    return 0;
}
