#include "SO6.hpp"
#include <sstream>

std::string SO6::serialize() const {
    std::ostringstream oss;
    for (const auto& elem : arr) {
        oss << elem.serialize() << " ";
    }
    oss << static_cast<int>(last_T) << " ";
    oss << sign_convention << " ";
    oss << hash << " ";
    for (const auto& col : Col) oss << static_cast<int>(col) << " ";
    for (const auto& row : Row) oss << static_cast<int>(row) << " ";
    return oss.str();
}

SO6 SO6::deserialize(const std::string& data) {
    std::istringstream iss(data);
    SO6 obj;
    for (auto& elem : obj.arr) {
        std::string elem_str;
        iss >> elem_str;
        elem = Z2::deserialize(elem_str);
    }
    int last_T_int;
    iss >> last_T_int;
    obj.last_T = static_cast<unsigned char>(last_T_int);
    uint16_t temp_sign_convention;
    iss >> temp_sign_convention;
    obj.sign_convention = temp_sign_convention;
    iss >> obj.hash;
    for (auto& col : obj.Col) { int v; iss >> v; col = static_cast<uint8_t>(v); }
    for (auto& row : obj.Row) { int v; iss >> v; row = static_cast<uint8_t>(v); }
    return obj;
}
