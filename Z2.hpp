
class Z2{
// elements of Z[1/sqrt(2)] are stored in the form (val[0] + val[1]*sqrt(2))/2^val[2]
public:
    Z2(const int8_t,const int8_t,const int8_t); // the ints paseed form the entries of val
    Z2();// the entries of val are all 0
    inline int8_t& operator[](int8_t i){return val[i];} //returns ith component of val
    inline const int8_t& operator[](int8_t i) const{return val[i];} //does the same but for const Z2
    Z2 operator+(Z2&); //handles addition
    Z2& operator+=(Z2&); //handles +=
    Z2 operator-(); //handles negation
    Z2 operator-(Z2&); //handles subtraction
    bool operator<(Z2&);
    bool operator<(const int8_t&);
    const bool operator<(const Z2& other) const;
    bool operator>(const int8_t&);
    bool operator>(Z2& other);
    bool operator<=(Z2& other);
    bool operator>=(Z2& other);
    Z2 operator*(const Z2&); //function that handles multiplication
    bool operator==(const Z2&); //function that checks equality between two Z2
    bool operator==(const int8_t&); //function that checks equality between two Z2
    bool operator!=(const Z2&); //function that checks equality between two Z2
    Z2& operator=(const Z2&); //function that makes the operator have equal entries to parameter
    Z2& reduce(); //auxiliary function to make sure every triad is in a consistent most reduced form
    int8_t* scale(const int8_t&); //auxiliary function to make sure that when addition is performed the exponents in the denominators are equal
    Z2 abs(); //Returns the absolute value
    //friend std::ostream& operator<<(std::ostream&,const Z2&); //display
    // float toFloat(); //Returns the Z2 object as a float
    int8_t getLDE() {return 0;}; //gives the denominator exponent of sqrt(2).
    void negate(){val[0]=-val[0];val[1]=-val[1];}
private:
    int8_t val[3]; //values of the Z2
    int8_t reg[3]; //workspace array
};
