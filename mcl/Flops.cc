/****************************************************************************************[Flops.cc]
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

#include "mcl/Flops.h"

using namespace Minisat;


void Flops::define(Gate flop, Sig next, Sig init){
    assert(type(flop) == gtype_Inp); // 'flop' must be an input.
    assert(!isFlop(flop));           // 'flop' may only be defined once.
    assert(flop != gate_Undef);
    assert(next != sig_Undef);
    assert(init != sig_Undef);

    static const FlopDef no_def   = { sig_Undef, sig_Undef };
    FlopDef              new_def  = { next, init };
    def_map.growTo(flop, no_def);
    def_map[flop] = new_def;

    flops.push(flop);
}


void Flops::moveTo    (Flops& to)       { def_map.moveTo(to.def_map); flops.moveTo(to.flops); }
void Flops::copyTo    (Flops& to) const { def_map.copyTo(to.def_map); flops.copyTo(to.flops); }
void Flops::clear     (bool dealloc)    { def_map.clear(dealloc); flops.clear(dealloc); }
