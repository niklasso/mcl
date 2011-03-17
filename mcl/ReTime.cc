/***************************************************************************************[ReTime.cc]
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

// NOTE: this needs to be re-written from scratch!
#if 0

#include "mcl/DagShrink.h"
#include "mcl/ReTime.h"

using namespace Minisat;

//=================================================================================================
// Helpers:

static inline void removeDeadFlops(Circ& c, Box& b, Flops& flp)
{
    vec<Gate> xs;
    GSet      reached_gates;

    // Calculate all gates reachable from some (real) output:
    //
    for (int i = 0; i < b.outs.size(); i++)
        xs.push(gate(b.outs[i]));

    while (xs.size() > 0){
        Gate g = xs.last(); xs.pop();

        if (reached_gates.has(g)) 
            continue;
        reached_gates.insert(g);

        assert(g != gate_Undef);
        if (type(g) == gtype_And){
            xs.push(gate(c.lchild(g)));
            xs.push(gate(c.rchild(g)));
        }else if (type(g) == gtype_Inp && flp.isFlop(g))
            xs.push(gate(flp.def(g)));
    }

    // int gates_before = c.nGates();
    // int flops_before = flp.size();
#ifndef NDEBUG    
    int inps_before  = b.inps.size();
    int outs_before  = b.outs.size();
#endif
    // Create reduced flop set:
    //
    Flops tmp_flops; 
    for (int i = 0; i < flp.size(); i++)
        if (reached_gates.has(flp[i]))
            tmp_flops.defineFlop(flp[i], flp.def(flp[i]));
        // else
        //     printf(" >> FLOP x%d not reached\n", index(flp[i]));
    tmp_flops.moveTo(flp);

    // for (int i = 0; i < flp.size(); i++){
    //     printf("CHECKING FLOP: %d, def = %d\n", index(flp[i]), index(gate(flp.def(flp[i]))));
    //     assert(flp.def(flp[i]) != sig_Undef);
    // }
    assert(inps_before == b.inps.size());
    assert(outs_before == b.outs.size());

    // Remove now dead logic:
    removeDeadLogic(c, b, flp);

    // fprintf(stderr, " >>> REMOVE DEAD FLOPS (before: gates=%d, flops=%d) (after: gates=%d, flops=%d, outs = %d)\n", 
    //         gates_before, flops_before, c.nGates(), flp.size(), outs_before);
}


static inline bool retimeable(const Circ& from, const Flops& from_flp, Gate g)
{
    assert(type(g) == gtype_And);

    Gate x = gate(from.lchild(g));
    Gate y = gate(from.rchild(g));

    return type(x) == gtype_Inp && from_flp.isFlop(x)
        && type(y) == gtype_Inp && from_flp.isFlop(y)
        // && (from.nFanouts(x) == 1 || from.nFanouts(y) == 1);
         && from.nFanouts(x) == 1 && from.nFanouts(y) == 1;
}


static inline bool hasInput(const Circ& from, Gate g, const GMap<Sig>& map)
{
    assert(type(g) == gtype_And);

    Gate x = gate(from.lchild(g));
    Gate y = gate(from.rchild(g));

    return map[x] != sig_Undef && map[y] != sig_Undef;
}


static inline bool tryReTime(const Circ& from, const Flops& from_flp, Circ& to, Flops& new_flops, Gate g, GMap<Sig>& map)
{
    assert(type(g) == gtype_And);
    assert(type(from.lchild(g)) == gtype_Inp && from_flp.isFlop(gate(from.lchild(g))));
    assert(type(from.rchild(g)) == gtype_Inp && from_flp.isFlop(gate(from.rchild(g))));

    bool init_val = sign(from.lchild(g)) && sign(from.rchild(g));
    Sig  x_from   = from_flp.def(gate(from.lchild(g))) ^ sign(from.lchild(g));
    Sig  y_from   = from_flp.def(gate(from.rchild(g))) ^ sign(from.rchild(g));

         if (map[gate(x_from)] == sig_Undef) return false;
    else if (map[gate(y_from)] == sig_Undef) return false;

    Sig  x_to     = map[gate(x_from)] ^ sign(x_from);
    Sig  y_to     = map[gate(y_from)] ^ sign(y_from);
    
    // {
    //     Sig x = from.lchild(g);
    //     Sig y = from.rchild(g);
    // 
    //     printf("retiming gate %d:\n", index(g));
    //     printf("...lchild: %s%d\n", sign(x)?"-":"", index(gate(x)));
    //     printf("...rchild: %s%d\n", sign(y)?"-":"", index(gate(y)));
    //     printf("...x_from: %s%d\\%d\n", sign(x_from)?"-":"", index(gate(x_from)), from.nFanouts(gate(x)));
    //     printf("...y_from: %s%d\\%d\n", sign(y_from)?"-":"", index(gate(y_from)), from.nFanouts(gate(y)));
    //     printf("...x_to  : %s%d\n", sign(x_to)?"-":"", index(gate(x_to)));
    //     printf("...y_to  : %s%d\n", sign(y_to)?"-":"", index(gate(y_to)));
    // }

    Sig  next_sig = to.mkAnd(x_to, y_to);
    Gate new_flop = gate(to.mkInp());
    map[g]        = mkSig(new_flop, init_val);

    new_flops.defineFlop(new_flop, next_sig ^ init_val);

    return true;
}


static inline void mergeEqualFlops(Circ& c, Box& b, Flops& flp)
{
    Circ      to;
    GMap<Sig> m; 
    m.growTo(c.lastGate(), sig_Undef);

    // Copy inputs (including flop gates):
    for (int i = 0; i < b.inps.size(); i++) m[b.inps[i]] = to.mkInp();
    for (int i = 0; i < flp.size(); i++)    m[flp[i]]    = to.mkInp();

    vec<Gate> keep_flops;
    for (int i = 0; i < flp.size(); i++){
        for (int j = 0; j < i; j++)
            if (flp.def(flp[j]) == flp.def(flp[i])){
                m[flp[i]] = m[flp[j]];
                // printf(" EQUAL FLOPS FOUND (%d = %s%d, %d = %s%d)!\n", 
                //        index(flp[i]), sign(flp.def(flp[i]))?"-":"", index(gate(flp.def(flp[i]))), 
                //        index(flp[j]), sign(flp.def(flp[j]))?"-":"", index(gate(flp.def(flp[j])))
                //        );
                goto next_flop;
            }
        keep_flops.push(flp[i]);
    next_flop:;
    }

    m[gate_True] = sig_True;
    for (Gate g = c.firstGate(); g != gate_Undef; g = c.nextGate(g))
        if (type(g) == gtype_And)
            m[g] = to.mkAnd(m[gate(c.lchild(g))] ^ sign(c.lchild(g)),
                            m[gate(c.rchild(g))] ^ sign(c.rchild(g))
                            );

    // Remap inputs, outputs and flops:
    map(m, b);
    Flops to_flops;
    for (int i = 0; i < keep_flops.size(); i++){
        Gate g = keep_flops[i];
        Sig  x = flp.def(g);
        assert(!sign(m[g]));
        to_flops.defineFlop(gate(m[g]), m[gate(x)] ^ sign(x));
    }

    // Move circuit back:
    to      .moveTo(c);
    to_flops.moveTo(flp);
}

//=================================================================================================
// Functions for moving (pushing/pulling) flops in circuits:


void Minisat::fwdReTime(Circ& c, Box& b, Flops& flp)
{
    mergeEqualFlops(c, b, flp);
    removeDeadFlops(c, b, flp);

    const int max_iters = 3;
    bool shrunk = true;
    for (int iter = 0; shrunk && iter < max_iters; iter++){
        Circ      to;
        Flops     new_flops;
        GMap<Sig> m; 
        m.growTo(c.lastGate(), sig_Undef);

        // printf("starting iteration\n");

        // Copy inputs (including flop gates):
        for (int i = 0; i < b.inps.size(); i++) m[b.inps[i]] = to.mkInp();
        for (int i = 0; i < flp.size(); i++)    m[flp[i]]    = to.mkInp();

        // Increase fanouts for all root signals:
        for (int i = 0; i < b.outs.size(); i++) c.bumpFanout(gate(b.outs[i]));
        for (int i = 0; i < flp.size(); i++)    c.bumpFanout(gate(flp.def(flp[i])));

        shrunk = false;
        m[gate_True] = sig_True;
        for (Gate g = c.firstGate(); g != gate_Undef; g = c.nextGate(g))
            if (type(g) == gtype_And)
                if (!retimeable(c, flp, g) && hasInput(c, g, m))
                    m[g] = to.mkAnd(m[gate(c.lchild(g))] ^ sign(c.lchild(g)),
                                    m[gate(c.rchild(g))] ^ sign(c.rchild(g))
                                    );

        for (Gate g = c.firstGate(); g != gate_Undef; g = c.nextGate(g))
            if (type(g) == gtype_And)
                if (retimeable(c, flp, g) && tryReTime(c, flp, to, new_flops, g, m))
                    shrunk = true;
                else{
                    assert(hasInput(c, g, m));
                    m[g] = to.mkAnd(m[gate(c.lchild(g))] ^ sign(c.lchild(g)),
                                    m[gate(c.rchild(g))] ^ sign(c.rchild(g))
                                    );
                }

        // Remap inputs, outputs and flops:
        map(m, b);
        map(m, flp);

        // Move circuit back:
        to.moveTo(c);

        // Attach new flops:
        for (int i = 0; i < new_flops.size(); i++){
            Gate g = new_flops[i];
            Sig  d = new_flops.def(new_flops[i]);
            flp.defineFlop(g, d);
        }

        // Remove dead flops and logic:
        //
        mergeEqualFlops(c, b, flp);
        removeDeadFlops(c, b, flp);
    }
}

#endif
