/*****************************************************************************************[Flops.h]
Copyright (c) 2007-2011, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef Minisat_Flops_h
#define Minisat_Flops_h

#include "mcl/Circ.h"

namespace Minisat {

//=================================================================================================
// Flops -- a class for representing initial and step behavior of a set of flop gates.
//
//   Maintains maps from flop-gates to their initialization and step functions. The initialization
//   and step functions are expected to live in separate circuit environments, but this requirement
//   may be relaxed in the future. For the special case that all flops are initialized with
//   constants, there does not have to be a separate circuit for initial values.

class Flops
{
public:
    // Define the next state of the input 'flop' to be 'next' and the initial value to be 'init'.
    void define    (Gate flop, Sig next, Sig init = sig_False);

    // Readers:
    Sig  next      (Gate flop) const;
    Sig  init      (Gate flop) const;
    bool isFlop    (Gate g)    const;

    // Iterate over the 'flop' input gates:
    int  size      ()          const;
    Gate operator[](int i)     const;

    // Move/Copy/Clear:
    void moveTo    (Flops& to);
    void copyTo    (Flops& to) const;
    void clear     (bool dealloc = false);

private:
    struct FlopDef { Sig next, init; };
    GMap<FlopDef> def_map;
    vec<Gate>     flops;
};

//=================================================================================================
// Implementation of inline methods:

inline Sig  Flops::next      (Gate flop) const { assert(isFlop(flop)); return def_map[flop].next; }
inline Sig  Flops::init      (Gate flop) const { assert(isFlop(flop)); return def_map[flop].init; }
inline bool Flops::isFlop    (Gate g)    const { return def_map.has(g) && def_map[g].next != sig_Undef; }
inline int  Flops::size      ()          const { return flops.size(); }
inline Gate Flops::operator[](int i)     const { return flops[i]; }

//=================================================================================================

};

#endif
