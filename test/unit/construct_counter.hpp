/*
 * Constructor/Destructor call counting
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#include "types.hpp"

// Count calls to the constructor and destructor of this class.
//
// This is a useful helper to test whether library code properly frees
// resources.
//
// We use a tagged class to get distinct counters. To use this, create an empty
// tag class in a local scope.
template <typename TAG> class construct_counter
{
public:
    static size_t constructed;
    static size_t destructed;

    construct_counter() { constructed++; }
    ~construct_counter() { destructed++; }
};

template <typename TAG> size_t construct_counter<TAG>::constructed{0};

template <typename TAG> size_t construct_counter<TAG>::destructed{0};
