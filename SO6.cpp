#include <algorithm>
#include <iostream>
#include <vector>
#include <array>
#include <sstream>
#include <bitset>
#include <iterator>
#include <stdint.h>
#include "Z2.hpp"
#include "SO6.hpp"


/**
 * Method to compare two Z2 arrays of length 6 lexicographically
 * @param first array of Z2 of length 6
 * @param second array of Z2 of length 6
 * @return -1 if first < second, 0 if equal, 1 if first > second
 */
bool lexLess (Z2 first[6],Z2 second[6]) {
    for(int8_t i = 0; i < 6 ; i++) {
        if(first[i] < second[i]) return true;
        if(second[i] < first[i]) return false;
    }
    return false;
}

/**
 * @brief Method to avoid multiple calls to lexLess when we need to lexicographically compare two strings
 * It doesn't seem like this will hit 0, since this only used when sorting, I think...
 * 
 * @param first 
 * @param second 
 * @return int8_t 
 */
int lexComp (const Z2 first[6], const Z2 second[6]) {
    for(int8_t i = 0; i < 6 ; i++) {
        if(first[i] < second[i]) return -1;
        if(second[i] < first[i]) return 1;
    }
    return 0;
}

/**
 * Basic constructor. Initializes Zero matrix.
 *
 */
SO6::SO6(){
    for(int8_t i=0; i<6; i++){
        for(int8_t j=0; j<6; j++)
            arr[i][j]=Z2();
    }
    hist = {};
}

/**
 * Constructor that initializes arbitrary matrix with arbitrary name
 * @param t the object history
 */
SO6::SO6(std::vector<int8_t> t){
    for(int8_t i=0; i<6; i++){
        for(int8_t j=0; j<6; j++)
            arr[i][j]=Z2();
    }
    hist = t;
}

/**
 * Constructor that initializes arbitrary matrix with arbitrary name
 * @param a array of Z2 that the SO6 will take as values
 * @param t the object history
 */
SO6::SO6(Z2 a[6][6], std::vector<int8_t> t){
    // initializes SO6's entries according to a
    for(int8_t col = 0; col<6; col++){
        for(int8_t row = 0; row<6; row++) {
            arr[col][row] = a[row][col];
        }
    }
    fixSign();
    lexOrder();
    hist = t;
}

/**
 * Overloads the * operator with matrix multiplication for SO6 objects
 * @param other reference to SO6 to be multiplied with (*this)
 * @return matrix multiplication of (*this) and other
 */
SO6 SO6::operator*(SO6& other){
    //multiplies operators assuming COLUMN,ROW indexing
    SO6 prod;
    Z2 next;
    
    // Compute product
    for(int8_t row=0; row<6; row++){
        for(int8_t col = 0; col<6; col++){
            for(int8_t k = 0; k<6; k++){
                // next = arr[row][k]*other[col][k];            // This transpose * other
                next = arr[k][row]*other[col][k];            // This not transpose * other
                // prod(col,row) += next;
                prod[col][row] += next;
            }
        }
    }
    prod.fixSign();
    prod.lexOrder();
    prod.hist = hist;
    prod.hist.insert(prod.hist.end(), other.hist.begin(), other.hist.end());
    return prod;
}

SO6 SO6::transpose() {
    return SO6(arr, hist);
}

void SO6::fixSign() {
    for(int8_t col = 0; col<6; col++){
        for(int8_t row = 0; row < 6; row++) {
            if(arr[col][row] < 0) {
                while(row<6) arr[col][row++] = -arr[col][row];
            }
            else if(arr[col][row] == Z2(0,0,0)) continue;
            break;
        }
    }
}


// This may be slow
void SO6::lexOrder() {
    Z2* myZ2[] = {arr[0],arr[1],arr[2],arr[3],arr[4],arr[5]};
    std::vector<Z2*> myvector(myZ2,myZ2+6);
    std::sort (myvector.begin(), myvector.end(),lexLess);
    Z2 arr2[6][6];
    for(int8_t i = 0; i<6; i++) {
        for(int8_t j = 0; j<6; j++) {
            arr2[i][j] = (myvector.at(i))[j];
        }
    }
    for(int8_t i = 0; i<6; i++) {
        for(int8_t j = 0; j<6; j++) {
            arr[i][j] = arr2[i][j];
        }
    }
}

bool SO6::operator<(const SO6 &other) const {
    for(int8_t col = 0; col < 6; col++) {
        switch (lexComp((*this)[col],other[col])) {   
            case -1: return true;
            case 1: return false;
        }
    }
    return false;
}

/** overloads == method to check equality of SO6 matrices
 *  @param other reference to SO6 to be checked against
 *  @return whether or not (*this) and other are equivalent
 */
bool SO6::operator==(SO6 &other) {
    // SO6 are the same if they have the same triangle
    // TODO: lower right triangle seems super fast, but can try out others                
    for(int8_t col = 5; col>-1 ; col--) {                                
        for(int8_t row = 5; row>5-col-1; row--) {      
            if(arr[col][row]!=other[col][row]) return false;
        }
    }
    return true;
}

/**
 * Overloads << function for SO6.
 * @param os reference to ostream object needed to implement <<
 * @param m reference to SO6 object to be displayed
 * @returns reference ostream with the matrix's display form appended
 */
std::ostream& operator<<(std::ostream& os, const SO6& m){
    for (int8_t i : m.hist) {
        os << std::hex << +i;//<< " ";
    }
    //os << "\n";
/*     for(int row = 0; row<6; row++){
        os << '[';
        for(int col = 0; col<6; col++)
            os << m[col][row] <<' ';
        os << "] \n";
    }
    os << "\n"; */
    return os;
}