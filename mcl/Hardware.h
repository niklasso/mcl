/**************************************************************************************[Hardware.h]
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

#ifndef Minisat_Hardware_h
#define Minisat_Hardware_h

#include "mcl/Circ.h"

namespace Minisat {

//=================================================================================================
// Miscellaneous functions for generating hardware such as integer arithmetics, sorting etc.

void fullAdder (Circ& c, Sig x, Sig y, Sig z, Sig& sum, Sig& carry);
void dadaAdder (Circ& c, vec<vec<Sig> >& columns, vec<Sig>& result);
void multiplier(Circ& c, vec<Sig>& xs, vec<Sig>& ys, vec<Sig>& result);
void squarer   (Circ& c, vec<Sig>& xs, vec<Sig>& result);

void fullAdderCorrect(void);
void multiplierCorrect(int size);
void factorize64(uint64_t number);
void factorize64squarer(uint64_t number);

//=================================================================================================

};

#endif
