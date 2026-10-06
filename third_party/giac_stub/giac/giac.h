// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <iostream>
#include <string>
#include <vector>

namespace giac {

class context;

struct gen {
    enum type { _INT_, _DOUBLE_, _SYMB_, _VECT_ };
    int type = _INT_;
    long long val = 0;
    std::string name;
    std::vector<gen> _vect;

    gen() = default;
    gen(int v) : type(_INT_), val(v) {}
    gen(long long v) : type(_INT_), val(v) {}
    gen(const std::string& s) : type(_SYMB_), name(s) {}
    gen(const std::vector<gen>& v) : type(_VECT_), _vect(v) {}

    std::string print(context* = nullptr) const {
        if (type == _INT_) return std::to_string(val);
        if (type == _SYMB_) return name;
        if (type == _VECT_) {
            std::string s = "{";
            for (std::size_t i = 0; i < _vect.size(); ++i) {
                if (i) s += ",";
                s += _vect[i].print();
            }
            s += "}";
            return s;
        }
        return "0";
    }
};

inline gen eval(const gen& g, context* = nullptr) { return g; }

}  // namespace giac
