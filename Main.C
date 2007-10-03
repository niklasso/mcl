/******************************************************************************************[Main.C]
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

#include "SimpSolver.h"
#include "Circ.h"
#include "Clausify.h"
#include "Aiger.h"

#include "Hardware.h"

#include <cstdio>

void basicCircTest()
{
    Circ c;

    assert(~sig_Undef == sig_Error);
    assert(mkSig(gate_Undef, false) == sig_Undef);
    assert(mkSig(gate_Undef, true)  == sig_Error);

    vec<Sig> xs;

    xs.push(c.mkInp());
    xs.push(c.mkInp());
    xs.push(c.mkAnd(xs[0], xs[1]));
    xs.push(c.mkAnd(xs[1], xs[0]));
    assert(xs[2] == xs[3]);

    xs.push(c.mkAnd(xs[2], ~xs[3]));
    assert(xs[4] == sig_False);

    xs.push(c.mkAnd(xs[0], sig_True));
    xs.push(c.mkAnd(xs[0], sig_False));
    xs.push(c.mkAnd(sig_True,  xs[0]));
    xs.push(c.mkAnd(sig_False, xs[0]));
    assert(xs[5] == xs[0]);
    assert(xs[6] == sig_False);
    assert(xs[7] == xs[0]);
    assert(xs[8] == sig_False);

    SimpSolver             s;
    Clausifyer<SimpSolver> Cl(c, s);
    Cl.clausify(xs[2]);
    //s.toDimacs("fisk.cnf");

    printf("Basic Circ-test ok.\n");
}


int main(int argc, char** argv)
{
    basicCircTest();
    fullAdderCorrect();
    multiplierCorrect(4);

    //if (argc == 2)
    //    factorize64(atoll(argv[1]));

    return 0;
}
