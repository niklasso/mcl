/***************************************************************************************[SeqCirc.h]
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

#ifndef Minisat_SeqCirc_h
#define Minisat_SeqCirc_h

#include "mcl/Circ.h"
#include "mcl/Flops.h"

namespace Minisat {

//=================================================================================================
// SeqCirc -- a class for representing sequential circuits.

struct SeqCirc
{
    Circ  main;
    Circ  init;
    Flops flps;

    class InpIt : public Circ::InpIt {
        const Flops& flps;
    protected:
        void nextInput(){
            while (g != gate_Undef && flps.isFlop(g)){
                Circ::InpIt::operator++();
                Circ::InpIt::nextInput();
            }
        }
    public:
        InpIt(const SeqCirc& _sc, Gate _g) : Circ::InpIt(_sc.main,_g), flps(_sc.flps) { nextInput(); }
        InpIt operator++() { Circ::InpIt::operator++(); nextInput(); return *this; }
    };

    class FlopIt : public Circ::InpIt {
        const Flops& flps;
    protected:
        void nextFlop(){
            while (g != gate_Undef && !flps.isFlop(g)){
                Circ::InpIt::operator++();
                Circ::InpIt::nextInput();
            }
        }
    public:
        FlopIt(const SeqCirc& _sc, Gate _g) : Circ::InpIt(_sc.main,_g), flps(_sc.flps) { nextFlop(); }
        FlopIt operator++() { Circ::InpIt::operator++(); nextFlop(); return *this; }
    };

    InpIt  inpBegin() const { return InpIt(*this, gate_True /* FIXME: Is this weird? */); }
    InpIt  inpEnd  () const { return InpIt(*this, gate_Undef); }

    FlopIt flpsBegin() const { return FlopIt(*this, gate_True /* FIXME: Is this weird? */); }
    FlopIt flpsEnd  () const { return FlopIt(*this, gate_Undef); }

    void   clear    (){
        main.clear();
        init.clear();
        flps.clear();
    }
};

//=================================================================================================

};

#endif
