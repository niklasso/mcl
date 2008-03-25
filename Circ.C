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

#include "utils/Options.h"
#include "circ/Circ.h"

using namespace Minisat;

//=================================================================================================
// Circ options:

static const char* _cat = "CIRC";

static IntOption opt_rewrite_mode (_cat, "rw-mode", "Rewrite level. 0=None, 1=Strash+1-Level RWs, 2=Strash+2-Level RWs.", 2, IntRange(0, 2));

//=================================================================================================
// Circ members:


Circ::Circ() 
    : n_inps       (0) 
    , n_ands       (0)
    , strash       (NULL)
    , strash_cap   (0) 
    , tmp_gate     (gate_True)
    , rewrite_mode (opt_rewrite_mode)
{ 
    gates.growTo(tmp_gate); 
    restrashAll();
    gates[tmp_gate].strash_next = gate_Undef;
}


void Circ::clear()
{
    gates.clear();
    n_fanouts.clear();
    n_inps = 0;
    n_ands = 0;
    if (strash) free(strash);
    strash = NULL;
    strash_cap = 0;
    
    gates.growTo(tmp_gate); 
    restrashAll();
    gates[tmp_gate].strash_next = gate_Undef;
}


void Circ::moveTo(Circ& to)
{
    gates.moveTo(to.gates);
    n_fanouts.moveTo(to.n_fanouts);
    to.n_inps = n_inps;
    to.n_ands = n_ands;
    if (to.strash) free(to.strash);
    to.strash = strash;
    to.strash_cap = strash_cap;

    n_inps = 0;
    n_ands = 0;
    strash = NULL;
    strash_cap = 0;

    gates.growTo(tmp_gate); 
    restrashAll();
    gates[tmp_gate].strash_next = gate_Undef;
}


void Circ::push()  { gate_lim.push(gates.size()); }
void Circ::commit(){ gate_lim.pop(); }
void Circ::pop()
{
    assert(gate_lim.size() > 0);
    while ((uint32_t)gates.size() > gate_lim.last()){
        Gate g = gateFromId(gates.size()-1);
        if (type(g) == gtype_And){
            strashRemove(mkGate(gates.size()-1, gtype_And));

            // Update fanout counters:
            if (n_fanouts[gate(lchild(g))] < 255) n_fanouts[gate(lchild(g))]--; // else fprintf(stderr, "WARNING! fanout counter size exceded.\n");
            if (n_fanouts[gate(rchild(g))] < 255) n_fanouts[gate(rchild(g))]--; // else fprintf(stderr, "WARNING! fanout counter size exceded.\n");
            
            n_ands--;
        }else
            n_inps--;
        gates.shrink(1);
    }
    gate_lim.pop();
}


void Circ::restrashAll()
{
    static const unsigned int nprimes   = 47;
    static const unsigned int primes [] = { 31, 47, 71, 107, 163, 251, 379, 569, 853, 1279, 1931, 2897, 4349, 6529, 9803, 14713, 22073, 33113, 49669, 74507, 111767, 167663, 251501, 377257, 565889, 848839, 1273267, 1909907, 2864867, 4297301, 6445951, 9668933, 14503417, 21755137, 32632727, 48949091, 73423639, 110135461, 165203191, 247804789, 371707213, 557560837, 836341273, 1254511933, 1881767929, 2822651917U, 4233977921U };

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
//
bool Minisat::evaluate(const Circ& c, Sig x, GMap<lbool>& values)
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



//=================================================================================================
// Generate bottomUp topological orders:
//
void Minisat::bottomUpOrder(const Circ& c, Sig  x, GSet& gset) { bottomUpOrder(c, gate(x), gset); }
void Minisat::bottomUpOrder(const Circ& c, Gate g, GSet& gset)
{
    if (gset.has(g) || g == gate_True) return;

    if (type(g) == gtype_And){
        bottomUpOrder(c, gate(c.lchild(g)), gset);
        bottomUpOrder(c, gate(c.rchild(g)), gset);
    }
    gset.insert(g);
}


void Minisat::bottomUpOrder(const Circ& c, const vec<Gate>& gs, GSet& gset)
{
    for (int i = 0; i < gs.size(); i++)
        bottomUpOrder(c, gs[i], gset);
}


void Minisat::bottomUpOrder(const Circ& c, const vec<Sig>& xs, GSet& gset)
{
    for (int i = 0; i < xs.size(); i++)
        bottomUpOrder(c, xs[i], gset);
}


// FIXME: remove or update when needed
void Minisat::bottomUpOrder(const Circ& c, const vec<Gate>& latches, const GMap<Sig>& latch_defs, GSet& gset)
{
    bool repeat;
    do {
        repeat = false;
        for (int i = 0; i < latches.size(); i++){
            Gate g = latches[i];
            Gate d = gate(latch_defs[g]);
            
            if (gset.has(g) && !gset.has(d)){
                bottomUpOrder(c, d, gset);
                repeat = true;
            }
        }
    } while (repeat);
}

//=================================================================================================
// Calculate circuit statistics:
//

void Minisat::circInfo(Circ& c, Gate g, GSet& reachable, int& n_ands, int& n_xors, int& n_muxes, int& tot_ands)
{
    if (reachable.has(g) || g == gate_True) return;

    reachable.insert(g);

    Sig x, y, z;

    vec<Sig> xs; xs.clear();

    if (c.matchXors(g, xs)){
        n_xors++;
        for (int i = 0; i < xs.size(); i++)
            circInfo(c, gate(xs[i]), reachable, n_ands, n_xors, n_muxes, tot_ands);
    }else if (c.matchMux(g, x, y, z)){
        n_muxes++;
        circInfo(c, gate(x), reachable, n_ands, n_xors, n_muxes, tot_ands);
        circInfo(c, gate(y), reachable, n_ands, n_xors, n_muxes, tot_ands);
        circInfo(c, gate(z), reachable, n_ands, n_xors, n_muxes, tot_ands);
    }else if (type(g) == gtype_And){
        n_ands++;
        c.matchAnds(g, xs);
        for (int i = 0; i < xs.size(); i++)
            circInfo(c, gate(xs[i]), reachable, n_ands, n_xors, n_muxes, tot_ands);
        tot_ands += xs.size();
    }
}


//=================================================================================================
// Copy the fan-in of signals, from one circuit to another:
//

static        Sig _copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map);
static inline Sig _copySig (const Circ& src, Circ& dst, Sig  x, GMap<Sig>& copy_map){ return _copyGate(src, dst, gate(x), copy_map) ^ sign(x); }
static        Sig _copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map)
{
    if (copy_map[g] == sig_Undef)
        if (type(g) == gtype_Inp)
            copy_map[g] = dst.mkInp();
        else 
            copy_map[g] = dst.mkAnd(_copySig(src, dst, src.lchild(g), copy_map), 
                                    _copySig(src, dst, src.rchild(g), copy_map));

    return copy_map[g];
}


Sig  Minisat::copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map) { 
    src.adjustMapSize(copy_map, sig_Undef); return _copyGate(src, dst, g, copy_map); }
Sig  Minisat::copySig (const Circ& src, Circ& dst, Sig  x, GMap<Sig>& copy_map) {
    src.adjustMapSize(copy_map, sig_Undef); return _copySig (src, dst, x, copy_map); }
void Minisat::copySig (const Circ& src, Circ& dst, const vec<Sig>& xs, GMap<Sig>& copy_map)
{
    src.adjustMapSize(copy_map, sig_Undef); 
    for (int i = 0; i < xs.size(); i++)
        _copySig(src, dst, xs[i], copy_map);
}
