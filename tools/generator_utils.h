/*
 *
 *  D-Bus++ - C++ bindings for D-Bus
 *
 *  Copyright (C) 2005-2007  Paolo Durante <shackan@gmail.com>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __DBUSXX_TOOLS_GENERATOR_UTILS_H
#define __DBUSXX_TOOLS_GENERATOR_UTILS_H

#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

struct Ty;
std::ostream& operator<<(std::ostream &os, Ty const &ty);

// Added for representing classes with possible template parameters
struct Ty {
        std::string ident; // Name/identifier of the class, including namespaces etc.
        // TODO: rename to tplParams!
        std::vector<Ty> tplParams; // Possible extra template parameters, e.g. vector<int, bool> would
                              // have the tplParams int and bool
        inline std::string str() {
                std::stringstream ss;
                ss << *this;
                return ss.str();
        }

        Ty(std::string s) : ident{s}, tplParams{} {}
        Ty() : ident{}, tplParams{} {}
};

inline std::ostream& operator<<(std::ostream &os, Ty const &ty)
{
        os << ty.ident;
        if (ty.tplParams.size() != 0) {
                os << "< ";
                std::stringstream ss;
                for (auto param : ty.tplParams) {
                        ss << param << ", ";
                }
                std::string result = ss.str();
                result = result.substr(0, result.length() - 2);
                os << result;
                os << " >";
        }
        return os;
}

const char *atomic_type_to_string(char t);
std::string stub_name(std::string name);
std::string signature_to_type(const std::string &signature);
// Our own signature to type conversion
Ty signature_to_ty(const std::string &signature);
// Our own check for simple type, allows std::string as simple types
bool is_simple_type(const std::string &signature);
bool is_primitive_type(const std::string &signature);
void _parse_signature(const std::string &signature, std::string &type, unsigned int &i);
void underscorize(std::string &str);
std::string legalize(const std::string &str);
std::string make_string(unsigned int v);

#endif//__DBUSXX_TOOLS_GENERATOR_UTILS_H
