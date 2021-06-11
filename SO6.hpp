
class SO6{
public:
    SO6();
    // SO6(std::string); //initializes zero matrix
    // SO6(Z2[6][6], std::string); //initializes matrix according to a 6x6 array of Z2
    SO6(std::vector<int8_t> t);
    SO6(Z2[6][6], std::vector<int8_t> t); //initializes matrix according to a 6x6 array of Z2
    SO6 operator*(SO6&); //mutliplication
    void fixSign();
    void lexOrder();
    inline Z2& operator()(int8_t col, int8_t row){return arr[col][row];} //returns the (i,j)th entry
    // bool operator%(SO6&);
    bool operator<(const SO6&) const;
    const Z2& operator()(int8_t i, int8_t j) const {return arr[i][j];} //returns the (i,j)th entry but for const
    bool operator==(SO6&); //checking equality up to signed permutation
    Z2* operator[](const int8_t i) {return arr[i];}  // Return the array element needed. 
    const Z2* operator[](const int8_t i) const {return arr[i];}  // Return the array element needed. 
    // inline std::string getName(){return(name);} //getter for Name
    // inline void setName(std::string newName){name = newName;}
    int8_t getLDE(){return(0);} //getter for LDE
    SO6 transpose();
    // void genOneNorm(); //Returns 1-norm of the first row
    // bool isPerm(SO6 s); //Returns true if and only if s is similar to this object
    // inline float normFloat() { return norm.toFloat(); }
    void genLDE(); //generates LDE, called after multiplication and constructor
    friend std::ostream& operator<<(std::ostream&,const SO6&); //display
    bool operator==(const SO6 &other) const;
private:
    Z2 arr[6][6];
    std::vector<int8_t> hist;
    // std::string name;
    // Z2 norm;
    // int8_t LDE;
};
