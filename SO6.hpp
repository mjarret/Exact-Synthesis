#ifndef SO6_HPP
#define SO6_HPP

#include <vector>
#include <map>
#include <optional>
#include <compare>
#include <concepts>
#include <bitset>
#include "Z2.hpp"
#include "pattern.hpp"

class pattern;

class SO6{
    public:
        SO6();
        SO6(Z2[6][6]); //initializes matrix according to a 6x6 array of Z2
        SO6(pattern &); //initializes matrix according to a pattern
        SO6 operator*(const SO6&) const; //mutliplication
        SO6 operator*(const pattern &) const;
        bool operator<(const SO6 &) const;

        inline int get_index(const int &row, const int &col) const {return (col<<2) + (col<<1) + row;}
        Z2* operator[](const int &col) {return arr + get_index(0,col);}  // Return the array element needed.
        inline Z2& get_element(const int &row, const int &col) {return arr[get_index(row,col)];}  // Return the array element needed.
        inline const Z2 get_element(const int &row, const int &col) const {return arr[get_index(row,col)];}  // Return the array element needed.
        Z2& get_lex_element(const int &row, const int &col) {return arr[get_index(Row[row],Col[col])];}  // Return the array element needed.
        const Z2 get_lex_element(const int &row, const int &col) const {return arr[get_index(Row[row],Col[col])];}  // Return the array element needed.
        const Z2* operator[](const int &col) const {return arr + get_index(0,col);}  // Return the array element needed. 
        friend std::ostream& operator<<(std::ostream&,const SO6&); //display

        SO6 left_multiply_by_circuit(std::vector<unsigned char> &);
        SO6 left_multiply_by_T_transpose(const int &);        

        const z2_int getLDE() const;
        pattern to_pattern() const;
        SO6 transpose();
        std::string name(); 
        std::string circuit_string(); 
        static SO6 reconstruct_from_circuit_string(const std::string& );        
        
        void update_history(const unsigned char &); 
        
        void row_permute(int *);
        void unpermuted_print() const;
        void unpermuted_print(const std::bitset<6> &) const;
        void canonical_form();
        void canonical_form_test();

        int get_pivot_column(std::bitset<6>&, std::unordered_map<std::bitset<6>,int>&);
        
        SO6 reconstruct();
        static SO6 reconstruct(const std::string &);
        // SO6 reconstruct(const std::string &);
        static std::string name_as_num(const std::string);
        
        std::strong_ordering lex_order(const int &, const int &, const int &) const;
        
        template <bool first_is_negative, bool second_is_negative> static bool lex_comparator(const Z2 &, const Z2 &);

        static const SO6 identity() {
            SO6 I;
            for(int k =0; k<6; k++) {
                I.arr[(k<<2) + (k<<1) + k] = 1;
                I.row_frequency[k][Z2(1,0,0)] = 1;
                I.row_frequency[k][Z2(0,0,0)] = 5;
            }
            return I;
        }
        uint8_t Col[6] = {0,1,2,3,4,5};
        uint8_t Row[6] = {0,1,2,3,4,5};

        
        
        void row_sort();
        std::vector<std::vector<int>> get_equivalence_classes() const;

        void build_row_from_ec(const std::vector<std::vector<int>>&);
        void find_largest_col_perm(SO6 &);
        bool get_next_equivalence_class(std::vector<std::vector<int>>&);
        void apply_row_permutation();
        void negate_row(int &);


        SO6 left_multiply_by_T(const int &) const;
        
        bool submatrix_lex_less(std::vector<int> &, std::vector<int> &, int);

        template<int i> requires(i >= 0 && i < 15) 
        static SO6 left_multiply_by_T(SO6 &S) {
            int row1, row2;
            unsigned char p;

            switch (i) {
                case 0: row1 = 0; row2 = 1; p = 1; break;
                case 1: row1 = 0; row2 = 2; p = 2; break;
                case 2: row1 = 0; row2 = 3; p = 3; break;
                case 3: row1 = 0; row2 = 4; p = 4; break;
                case 4: row1 = 0; row2 = 5; p = 5; break;
                case 5: row1 = 1; row2 = 2; p = 6; break;
                case 6: row1 = 1; row2 = 3; p = 7; break;
                case 7: row1 = 1; row2 = 4; p = 8; break;
                case 8: row1 = 1; row2 = 5; p = 9; break;
                case 9: row1 = 2; row2 = 3; p = 10; break;
                case 10: row1 = 2; row2 = 4; p = 11; break;
                case 11: row1 = 2; row2 = 5; p = 12; break;
                case 12: row1 = 3; row2 = 4; p = 13; break;
                case 13: row1 = 3; row2 = 5; p = 14; break;
                case 14: row1 = 4; row2 = 5; p = 15; break;
            }

            // Now we only have one method that uses the calculated row1, row2, and p
            // #pragma unroll
            for (int col = 0; col < 6; col++)
            {
                // To track the column sum, begin by decreasing the size by the elements that will be modified
                if (S.row_frequency[row1][S.get_element(row1, col).abs()] == 1) {
                    S.row_frequency[row1].erase(S.get_element(row1, col).abs());
                } else {
                    S.row_frequency[row1][S.get_element(row1, col).abs()]--;
                }

                if (S.row_frequency[row2][S.get_element(row2, col).abs()] == 1) {
                    S.row_frequency[row2].erase(S.get_element(row2, col).abs());
                } else {
                    S.row_frequency[row2][S.get_element(row2, col).abs()]--;
                }

                // Update elements
                Z2 tmp1 = S.get_element(row1, col);
                Z2 tmp2 = S.get_element(row2, col);
                S.get_element(row1, col) += tmp2;
                S.get_element(row2, col) -= tmp1;
                S.get_element(row1, col).increaseDE();
                S.get_element(row2, col).increaseDE();

                // Update frequencies
                S.row_frequency[row1][S.get_element(row1, col).abs()]++;
                S.row_frequency[row2][S.get_element(row2, col).abs()]++;
            }

            S.canonical_form();
            S.update_history(p);
            return S;
        }
   
        void sort_physical_array();
        void physical_print() const;
        std::vector<unsigned char> hist;
        std::map<Z2,int> row_frequency[6];
        std::vector<std::vector<int>> ecs;

        // Custom iterator class
            class Iterator {
            public:
                Iterator(const SO6& so6, int index)
                    : so6_(so6), index_(index), Row_(so6.Row) {}

                Iterator(const SO6& so6, int index, const uint8_t* Row)
                    : so6_(so6), index_(index), Row_(Row) {}

                // Copy assignment operator
                Iterator& operator=(const Iterator& other) {
                    if (this != &other) {
                        index_ = other.index_;
                    }
                    return *this;
                }
                // Dereference operator to get the current element based on Row and Col permutation
                const Z2& operator*() const {
                    int row_index = Row_[index_ % 6];  // Row index based on current index
                    int col_index = so6_.Col[index_ / 6];  // Col index based on current index
                    return so6_.arr[(col_index << 2) + (col_index << 1) + row_index];  // Access based on permutations
                }

                // Increment operator to move to the next element
                Iterator& operator++() {
                    ++index_;
                    return *this;
                }

                // Comparison operator (for end of range checking)
                bool operator!=(const Iterator& other) const {
                    return index_ != other.index_;
                }

                // Comparison operator for equality
                bool operator==(const Iterator& other) const {
                    return index_ == other.index_;
                }
                // Decrement operator
                Iterator& operator--() {
                    --index_;
                    return *this;
                }

                // Add integer offset (for pointer arithmetic)
                Iterator operator+(int offset) const {
                    return Iterator(so6_, index_ + offset);
                }

                // Subtract integer offset (for pointer arithmetic)
                Iterator operator-(int offset) const {
                    return Iterator(so6_, index_ - offset);
                }

                // Difference between two iterators
                int operator-(const Iterator& other) const {
                    return index_ - other.index_;
                }

            private:
                const SO6& so6_;   // Reference to the SO6 object
                int index_;        // Current position in iteration
                const uint8_t *Row_;
            };

            // Begin iterator: Start from the first element
            Iterator begin() const {
                return Iterator(*this, 0);
            }

            // Begin iterator: Start from the first element
            Iterator begin(uint8_t* Row) const {
                return Iterator(*this, 0, Row);
            }

            // End iterator: After the last element
            Iterator end() const {
                return Iterator(*this, 36);  // End after 36 elements (6x6 matrix)
            }       
            // End iterator: After the last element
            Iterator end(uint8_t *Row) const {
                return Iterator(*this, 0, Row) +6;  // End after 6 elements (6x6 matrix)
            }       

            Z2 arr[36];
            bool is_better_permutation(const uint8_t*, const uint8_t*,const int & curr_sc);

            std::pair<SO6::Iterator,SO6::Iterator> get_column(const int & col) const {
                return std::pair<SO6::Iterator,SO6::Iterator>(Iterator(*this, (col << 2) + (col << 1)), Iterator(*this, (col << 2) + (col << 1) + 6));
            }

            std::pair<SO6::Iterator,SO6::Iterator> get_column(const int & col, const uint8_t* Row) const {
                return std::pair<SO6::Iterator,SO6::Iterator>(Iterator(*this, (col << 2) + (col << 1), Row), Iterator(*this, (col << 2) + (col << 1) + 6, Row));
            }

            // bool get_next_ec_permutation(std::vector<int>&);
            uint8_t sign_convention = 0;

            void unpermuted_print(const uint8_t[6], const uint8_t[6]) const;
    private:
        
};

#endif
