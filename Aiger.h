/*****************************************************************************************[Aiger.h]
Copyright (c) 2007, Niklas Sorensson

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

#ifndef Minisat_Aiger_h
#define Minisat_Aiger_h

#include "circ/Circ.h"

namespace Minisat {

//=================================================================================================
// Functions for parsing and printing circuits in the AIGER format. See <http://fmv.jku.at/aiger/>
// for specification of this format as well as supporting tools and example circuits.

struct AigerCirc {
    Circ       circ;
    vec<Gate>  inputs;
    vec<Gate>  latches;
    vec<Sig>   outputs;
    GMap<Sig>  latch_defs;
};

void readAiger (const char* filename,       Circ& c,       Box& b,       Flops& flp);
void writeAiger(const char* filename, const Circ& c, const Box& b, const Flops& flp);

//=================================================================================================

};

#endif
