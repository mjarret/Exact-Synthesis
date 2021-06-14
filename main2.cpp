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
    SO6 I;
    for(int8_t k =0; k<6; k++) {
        I(k,k) = one;
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
SO6 tMatrix(int8_t i, int8_t j, int8_t matNum){
    // Generates the T Matrix T[i+1, j+1]
    SO6 t({matNum});                               
    for(int8_t k = 0; k < 6; k++) t[k][k] = one;          // Initialize to the identity matrix
    t[i][i] = inverse_root2;                              // Change the i,j cycle to appropriate 1/sqrt(2)
    t[j][j] = inverse_root2;
    t[i][j] = inverse_root2;
    if(abs(i-j)!=1) t[i][j].negate();   
    t[j][i] = -t[i][j];
    t.fixSign();
    t.lexOrder();
    return(t);
}

/**
 * Returns the itemwise union of two vectors of vectors of SO6
 * @param a a vector of vectors of SO6
 * @param a another vector of vectors of SO6
 * @return m, where m[i] = a[i] union b[i]
 */
vector<vector<SO6>> mergedVect(vector<vector<SO6>>& a, vector<vector<SO6>>& b){
    //Takes two vectors of vectors of equal length a and b
    //and gives back the vector m where m[i] = a[i] union b[i]
    vector<vector<SO6>> m;
    vector<SO6> u;
    for(int i = 0; i<a.size(); i++){
        u = vector<SO6>();
        for(int j = 0; j<a[i].size(); j++)
            u.push_back(a[i][j]);
        for(int j = 0; j<b[i].size(); j++)
            u.push_back(b[i][j]);
        m.push_back(u);
    }
    return(m);
}

// Divides a vect of vects evenly over every LDE into numThreads vectors of vectors
vector<vector<vector<SO6>>> divideVect(vector<vector<SO6>>& toDivide){
    vector<vector<vector<SO6>>> toReturn(numThreads, vector<vector<SO6>>(toDivide.size()));
    int numPerThread;
    int remaining;
    for(int i = 0; i<toDivide.size(); i++){
        numPerThread = toDivide[i].size()/numThreads;
        for(int j = 0; j<numThreads; j++){
            for(int k = j*numPerThread; k<(j+1)*numPerThread; k++){
                toReturn[j][i].push_back(toDivide[i][k]);
            }
        }
        remaining = toDivide[i].size()-numPerThread*numThreads;
        for(int j = 0; j<remaining; j++){
            toReturn[j][i].push_back(toDivide[i][numPerThread*numThreads+j]);
        }
    }
    return toReturn;
}

/**
 * multiplies every entry of a vector of SO6 with every entry of another vector, and returns it sorted by LDE
 * @param batch small batch of various LDEs
 * @param tmats a vector of SO6 to multiply, we will always pass it the vector of the 15 T-Count 1 matrices though
 * @return a vector of vectors of SO6, containing the products of elements of tCounts and oneLDE, sorted by LDE
 */
vector<vector<SO6>> prodHelper(vector<vector<SO6>>& batch, vector<SO6>& tmats){
    // Takes one strata of LDEs from T count tCount, and multiplies by all T-count 1 matrices
    // Does NOT check for permutation
    int numLDES = batch.size()+1;
    SO6 prod;
    vector<vector<SO6>> toReturn(numLDES);
    for(int8_t i = 0; i<15; i++){
        for(int j = 0; j<batch.size(); j++){
            for(SO6 m : batch[j]){
                prod = tmats[i]*m;
                toReturn[prod.getLDE()].push_back(prod);
            }
        }
    }
    return(toReturn);
}


/**
 * Takes a vector of vectors of SO6, all with the same T-Count, broken out by LDE, and multiplies each element
 * by those of another vector of SO6 and returns them sorted by LDE as well
 * @param TminusOne the vector of vectors of SO6, sorted by LDE, to be multiplied
 * @param tcount the T-Count of TminusOne
 * @param tmats the vector of SO6 whose elements will be multiplying those of TminusOne
 * @return the product of all elements of TminusOne and tmats, sorted by LDE
 */
// vector<vector<SO6>> genAllProds(vector<vector<SO6>>& TminusOne, vector<SO6>& tmats, int numThreads){
//     vector<vector<vector<SO6>>> threadinput = divideVect(TminusOne, numThreads);
//     future<vector<vector<SO6>>> threads[numThreads];
//     vector<vector<SO6>> prod[numThreads];
//     for(int i = 0; i<numThreads; i++)
//         threads[i] = async(launch::async, prodHelper, ref(threadinput[i]), ref(tmats));
//     for(int i = 0; i<numThreads; i++){
//         prod[i] = threads[i].get();
//     }
//     vector<vector<SO6>> toReturn(prod[0].size());
//     for(int i = 0; i<numThreads; i++)
//         toReturn = mergedVect(toReturn, prod[i]);
//     return(toReturn);
// }
vector<vector<SO6>> genAllProds(vector<vector<SO6>>& TminusOne, vector<SO6>& tmats){
    vector<vector<vector<SO6>>> threadinput = divideVect(TminusOne);
    future<vector<vector<SO6>>> threads[numThreads];
    vector<vector<SO6>> prod[numThreads];
    for(int8_t i = 0; i<numThreads; i++)
        threads[i] = async(launch::async, prodHelper, ref(threadinput[i]), ref(tmats));
    for(int8_t i = 0; i<numThreads; i++){
        prod[i] = threads[i].get();
    }
    vector<vector<SO6>> toReturn(prod[0].size());
    for(int8_t i = 0; i<numThreads; i++)
        toReturn = mergedVect(toReturn, prod[i]);
    return(toReturn);
}


//int getLength(vector<vector<SO6>>& input){
//    int l = 0;
//    for(int i = 0; i<input.size(); i++){
//        l+= input[i].size();
//    }
//    return l;
//}


//Helper methods for pruneAllPerms

// //Prunes a vector of SO6 of matrices equivalent to check in indices between a and b
// void selfCheckHelper(vector<SO6>& vec, SO6& check, int a, int b){
//     for(int i = a; i<b; i++){
//         if(check==vec[i])
//             vec[i].setName("None"); //Marks for deletion
//     }
// }

/**
 * Returns whether or not an SO6 is contained in a vector of SO6
 * @param v the vector of SO6 to be checked for v
 * @param entry the SO6 to be compared to elements of v
 * @return whether or not v is contained in entry
 */
bool containedIn(vector<SO6>& v, SO6& entry){
    for(SO6 item: v){
        if(entry == item){
            return(true);
        }
    }
    return(false);
}

//Prunes a vector of SO6 of matrices equivalent to those in past in indices from a to b
void pastCheckHelper(vector<SO6>& vec, vector<SO6>& past, int a, int b){
    // Takes one strata of LDEs with LDE LDE, and then compares with relevant prior sets in lowerTs
    for(int i = a; i < b; i++){
        if(containedIn(past, vec[i]));
            // vec[i].setName("None"); //Marks for deletion
    }
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
    //generating list of T matrices
    //in the order Andrew wanted
    SO6 I = identity();
    set<SO6> ts;
    for(int8_t i = 0; i<15; i++) {
        if(i<5)       ts.insert(tMatrix(0,i+1,i));
        else if(i<9)  ts.insert(tMatrix(1, i-3,i));
        else if(i<12) ts.insert(tMatrix(2, i-6,i));
        else if(i<14) ts.insert(tMatrix(3, i-8,i));
        else          ts.insert(tMatrix(4,5,i));
    }


    set<SO6> prior;
    set<SO6> current({I});
    set<SO6> next;
    set<SO6> append;
    SO6 m;
    int8_t start = 0;

    
    vector<SO6> tsv; //t count 1 matrices
    for(int8_t i = 0; i<15; i++){
        if(i<5)
            tsv.push_back(tMatrix(0,i+1,i));
        else if(i<9)
            tsv.push_back(tMatrix(1, i-3,i));
        else if(i<12)
            tsv.push_back(tMatrix(2, i-6,i));
        else if(i<14)
            tsv.push_back(tMatrix(3, i-8,i));
        else
            tsv.push_back(tMatrix(4,5,i));
    }

    if(tIO && tCount > 2) {
        prior = fileRead(genFrom-2, tsv);
        current = fileRead(genFrom-1, tsv);
        start = genFrom - 1;
    }

    for(int8_t i = start; i<tCount; i++){
        std::cout<<"\nBeginning T-Count "<<(i+1)<<"\n";
        auto start = chrono::high_resolution_clock::now();
        next.clear();
        // Main loop here
        ifstream tfile;
        int8_t tsCount = 0;
        int size;
        long currentCount = 0;
        long save = 0;
        tfile.open(("data/T" + to_string(i + 1) + "index.txt").c_str());
        if (!tIO || !tfile) {
            if (tfile) {
                tfile.close();
            }
            for(SO6 t : ts) {
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
            }
        }
        else {
            next = fileRead(i+1, tsv);
            std::set<SO6>::iterator titr = ts.begin();
            std::set<SO6>::iterator citr = current.begin();
            string str;
            getline(tfile, str);
            stringstream s(str);
            getline(s, str, ' ');
            tsCount = stoi(str);
            getline(s, str, ' ');
            currentCount = stoi(str);
            tfile.close();
            advance(titr, tsCount);
            advance(citr, currentCount);
            SO6 t, curr;
            while (titr != ts.end()) {
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
        }
        // End main loop
        for(SO6 p : prior) {
            next.erase(p);                   // Erase T - 1
            append.erase(p);
        }
        auto end = chrono::high_resolution_clock::now();
        prior = current;                                    // T++
        current = next;                                     // T++

        // Begin reporting
        auto ret = chrono::duration_cast<chrono::milliseconds>(end-start).count();
        std::cout << ">>>Found " << next.size() << " new matrices in " << ret << "ms\n";

        // Write results out
        writeResults(i, tsCount, currentCount, append);
        append.clear();
    }
    chrono::duration<double> timeelapsed = chrono::high_resolution_clock::now() - tbefore;
    std::cout<< "\nTotal time elapsed: "<<chrono::duration_cast<chrono::milliseconds>(timeelapsed).count()<<"ms\n";
    return 0;
}
