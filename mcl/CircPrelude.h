/***********************************************************************************[CircPrelude.h]
Copyright (c) 2011, Niklas Sorensson

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

#ifndef Minisat_CircPrelude_h
#define Minisat_CircPrelude_h

#include <stdio.h>

#include "mcl/Circ.h"
#include "mcl/Equivs.h"

namespace Minisat {

//=================================================================================================
// Commonly useful utilities for circuits:


// Given certain values for inputs, calculate the values of all gates in the cone of influence
// of a signal:
bool evaluate     (const Circ& c, Sig x, GMap<lbool>& values);

// Calculate a bottom-up topological order starting at a gate:
void bottomUpOrder(const Circ& c, Gate g, GSet& gset);
void bottomUpOrder(const Circ& c, Sig  x, GSet& gset);
void bottomUpOrder(const Circ& c, const vec<Gate>& gs, GSet& gset);
void bottomUpOrder(const Circ& c, const vec<Sig>&  xs, GSet& gset);

// Copy gates from one circuit to another:
Sig  copyGate(const Circ& src, Circ& dst, Gate g,             GMap<Sig>& copy_map);
Sig  copySig (const Circ& src, Circ& dst, Sig  x,             GMap<Sig>& copy_map);
void copySig (const Circ& src, Circ& dst, const vec<Sig>& xs, GMap<Sig>& copy_map);
void copyCirc(const Circ& src, Circ& dst, GMap<Sig>& map, Gate stop_at = gate_Undef);
void copyCircWithSubst
             (const Circ& src, Circ& dst, GMap<Sig>& subst_map, GMap<Sig>& copy_map);
void copyCircWithSubst
             (const Circ& src, Circ& dst, const Equivs& subst, GMap<Sig>& copy_map);

void mkSubst (const Circ& c, const Equivs& eq, GMap<Sig>& subst);

//=================================================================================================
// Debug etc:

void printGate(Gate g);
void printSig (Sig x);
void printSigs(const vec<Sig>& xs);

//=================================================================================================

};

#endif
