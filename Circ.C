/******************************************************************************************[Circ.C]
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

#include "Circ.h"

//=================================================================================================
// Circ members:


static const unsigned int nprimes   = 47;
static const unsigned int primes [] = { 31, 47, 71, 107, 163, 251, 379, 569, 853, 1279, 1931, 2897, 4349, 6529, 9803, 14713, 22073, 33113, 49669, 74507, 111767, 167663, 251501, 377257, 565889, 848839, 1273267, 1909907, 2864867, 4297301, 6445951, 9668933, 14503417, 21755137, 32632727, 48949091, 73423639, 110135461, 165203191, 247804789, 371707213, 557560837, 836341273, 1254511933, 1881767929, 2822651917U, 4233977921U };

void Circ::restrashAll()
{
    // Find new size:
    unsigned int oldsize = strash_cap;
    strash_cap  = primes[0];
    for (unsigned int i = 1; strash_cap <= oldsize && i < nprimes; i++)
        strash_cap = primes[i];

    // printf("New strash size: %d\n", strash_cap);

    // Allocate and initialize memory for new table:
    strash = (Gate*)realloc(strash, sizeof(Gate) * strash_cap);
    for (unsigned int i = 0; i < strash_cap; i++)
        strash[i] = gate_Undef;

    // Rehash active and-nodes into new table:
    for (Gate g = firstGate(); g != gate_Undef; g = nextGate(g))
        if (type(g) == gtype_And) 
            strashInsert(g);
}

//=================================================================================================
// Circ utility functions:


// Given certain values for inputs, calculate the values of all gates in the cone of influence
// of a signal:
bool evaluate(const Circ& c, Sig x, GMap<lbool>& values)
{
    Gate g = gate(x);
    values.growTo(g, l_Undef);
    if (values[g] == l_Undef){
        assert(type(g) == gtype_And);
        values[g] = lbool(evaluate(c, c.lchild(g), values) && evaluate(c, c.rchild(g), values));
        //printf("%d = %s%d & %s%d ==> %d\n", index(g), sign(c.lchild(g)) ? "-":"", index(gate(c.lchild(g))), sign(c.rchild(g)) ? "-":"", index(gate(c.rchild(g))),
        //       toInt(values[g]));

    }
    assert(values[g] != l_Undef);
    return (values[g] ^ sign(x)) == l_True;
}



void bottomUpOrder(Circ& c, Sig  x, GSet& gset) { bottomUpOrder(c, gate(x), gset); }
void bottomUpOrder(Circ& c, Gate g, GSet& gset)
{
    if (gset.has(g)) return;

    if (type(g) == gtype_And){
        bottomUpOrder(c, gate(c.lchild(g)), gset);
        bottomUpOrder(c, gate(c.rchild(g)), gset);
    }
    gset.insert(g);
}


void bottomUpOrder(Circ& c, const vec<Gate>& gs, GSet& gset)
{
    for (int i = 0; i < gs.size(); i++)
        bottomUpOrder(c, gs[i], gset);
}


void bottomUpOrder(Circ& c, const vec<Sig>& xs, GSet& gset)
{
    for (int i = 0; i < xs.size(); i++)
        bottomUpOrder(c, xs[i], gset);
}


void bottomUpOrder(Circ& c, const vec<Def>& latch_defs, GSet& gset)
{
    bool repeat;
    do {
        repeat = false;
        for (int i = 0; i < latch_defs.size(); i++){
            Gate g = gate(latch_defs[i].var);
            Gate d = gate(latch_defs[i].def);
            
            if (gset.has(g) && !gset.has(d)){
                bottomUpOrder(c, d, gset);
                repeat = true;
            }
        }
    } while (repeat);
}
