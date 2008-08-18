/*************************************************************************************[CombSweep.h]
Copyright (c) 2008, Niklas Sorensson

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

#ifndef Minisat_CombSweep_h
#define Minisat_CombSweep_h

#include "core/Solver.h"
#include "simp/SimpSolver.h"
#include "circ/Circ.h"
#include "circ/Clausify.h"

namespace Minisat {

int combEqSweep(Circ& cin, Clausifyer<Solver>& cl, Solver& s, const Eqs& eqs_in, Eqs& eqs_out);
int combEqSweep(Circ& cin, Clausifyer<SimpSolver>& cl, SimpSolver& s, const Eqs& eqs_in, Eqs& eqs_out);

void makeUnitClass(const Circ& cin, Eqs& unit);

};

#endif
