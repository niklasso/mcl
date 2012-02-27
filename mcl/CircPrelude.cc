/**********************************************************************************[CircPrelude.cc]
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

#include "mcl/CircPrelude.h"

using namespace Minisat;

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


#if 0
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
#endif

//=================================================================================================
// Copy the fan-in of signals, from one circuit to another:
//

static        Sig _copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map);
static inline Sig _copySig (const Circ& src, Circ& dst, Sig  x, GMap<Sig>& copy_map){ return _copyGate(src, dst, gate(x), copy_map) ^ sign(x); }
static        Sig _copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map)
{
    if (copy_map[g] == sig_Undef){
        if (g == gate_True)
            copy_map[g] = sig_True;
        else if (type(g) == gtype_Inp)
            copy_map[g] = dst.mkInp(src.number(g));
        else{
            assert(type(g) == gtype_And);
            copy_map[g] = dst.mkAnd(_copySig(src, dst, src.lchild(g), copy_map), 
                                    _copySig(src, dst, src.rchild(g), copy_map));
        }
    }

    return copy_map[g];
}


Sig  Minisat::copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map) { 
    copy_map.growTo(src.lastGate(), sig_Undef); return _copyGate(src, dst, g, copy_map); }
Sig  Minisat::copySig (const Circ& src, Circ& dst, Sig  x, GMap<Sig>& copy_map) {
    copy_map.growTo(src.lastGate(), sig_Undef); return _copySig (src, dst, x, copy_map); }
void Minisat::copySig (const Circ& src, Circ& dst, const vec<Sig>& xs, GMap<Sig>& copy_map)
{
    copy_map.growTo(src.lastGate(), sig_Undef);
    for (int i = 0; i < xs.size(); i++)
        _copySig(src, dst, xs[i], copy_map);
}


//=================================================================================================
// Copy everything from one circuit to another:
//


void Minisat::copyCirc(const Circ& src, Circ& dst, GMap<Sig>& map, Gate stop_at)
{
    map.growTo(src.lastGate(), sig_Undef);

    map[gate_True] = sig_True;
    for (GateIt git = src.begin(); git != src.end(); ++git){
        if (map[*git] == sig_Undef){
            if (type(*git) == gtype_Inp)
                map[*git] = dst.mkInp(src.number(*git));
            else {
                assert(type(*git) == gtype_And);
                
                Sig ix = src.lchild(*git);
                Sig iy = src.rchild(*git);
                Sig ux = map[gate(ix)] ^ sign(ix);
                Sig uy = map[gate(iy)] ^ sign(iy);

                map[*git] = dst.mkAnd(ux, uy);
            }
        }
        if (stop_at != gate_Undef && *git == stop_at) break;
    }
}

//=================================================================================================
// This transformation copies the circuit 'src' into 'dst' while inlining the substitutions in
// 'subst_map'. The substitution map should be defined such that for all gates 'g' in 'src':
//
//   1) 'subst_map[g]' is a signal in 'src' which should replace 'g'. If the gate 'g' is to be kept
//   as it is, it is also ok to set 'subst_map[g]' to sig_Undef
//
//   2) If 'subst_map[g]' is different from sig_Undef, then 'subst_map[g]' must be smaller than 'g'
//   with respect to some circuit topological order.
//
// The resulting 'copy_map' gives the location of each gate in the destination circuit.
// 
// FIXME: The documentation for this function is only partial. What is left is to describe how "only
// occurences are substituted, but not the gates themselves."
#if 1
// TODO: Remove this (replaced by new version below).
void Minisat::copyCircWithSubst(const Circ& src, Circ& dst, GMap<Sig>& subst_map, GMap<Sig>& copy_map)
{
    subst_map.growTo(src.lastGate(), sig_Undef);
    copy_map .growTo(src.lastGate(), sig_Undef);

    copy_map[gate_True] = sig_True;
    for (GateIt git = src.begin(); git != src.end(); ++git)
        if (copy_map[*git] == sig_Undef){
            if (type(*git) == gtype_Inp)
                copy_map[*git] = dst.mkInp(src.number(*git));
            else {
                assert(type(*git) == gtype_And);
                
                Sig orig_x  = src.lchild(*git);
                Sig orig_y  = src.rchild(*git);
                Sig subst_x = subst_map[gate(orig_x)] == sig_Undef ? orig_x : subst_map[gate(orig_x)] ^ sign(orig_x);
                Sig subst_y = subst_map[gate(orig_y)] == sig_Undef ? orig_y : subst_map[gate(orig_y)] ^ sign(orig_y);
                Sig copy_x  = copy_map[gate(subst_x)] ^ sign(subst_x);
                Sig copy_y  = copy_map[gate(subst_y)] ^ sign(subst_y);

                copy_map[*git] = dst.mkAnd(copy_x, copy_y);
            }
        }
}
#endif

// New version: needs testing.
void Minisat::copyCircWithSubst(const Circ& src, Circ& dst, const Equivs& subst, GMap<Sig>& copy_map)
{
    copy_map .growTo(src.lastGate(), sig_Undef);

    copy_map[gate_True] = sig_True;
    for (GateIt git = src.begin(); git != src.end(); ++git)
        if (copy_map[*git] == sig_Undef){
            if (type(*git) == gtype_Inp)
                copy_map[*git] = dst.mkInp(src.number(*git));
            else {
                assert(type(*git) == gtype_And);
                
                Sig orig_x  = src.lchild(*git);
                Sig orig_y  = src.rchild(*git);
                Sig subst_x = subst.leader(orig_x);
                Sig subst_y = subst.leader(orig_y);
                assert(subst_x <= orig_x);
                assert(subst_y <= orig_y);
                Sig copy_x  = copy_map[gate(subst_x)] ^ sign(subst_x);
                Sig copy_y  = copy_map[gate(subst_y)] ^ sign(subst_y);

                copy_map[*git] = dst.mkAnd(copy_x, copy_y);
            }
        }
}


void Minisat::mkSubst(const Circ& c, const Equivs& eq, GMap<Sig>& subst)
{
    // Initialize to identity substitution:
    subst.clear();
    subst.growTo(c.lastGate(), sig_Undef);
    subst[gate_True] = sig_True;
    for (GateIt git = c.begin(); git != c.end(); ++git)
        subst[*git] = mkSig(*git);

    // Update identity substitution with given equivalences:
    for (uint32_t i = 0; i < eq.size(); i++){
        Sig leader = eq[i][0];
        assert(!sign(leader));
        for (int j = 1; j < eq[i].size(); j++){
            Sig x = eq[i][j];
            subst[gate(x)] = leader ^ sign(x);
        }
    }
}

//=================================================================================================
// Debug etc:

void Minisat::printSig(Sig x)
{
    if (x == sig_Undef)
        printf("x");
    else if (x == sig_True)
        printf("1");
    else if (x == sig_False)
        printf("0");
    else
        printf("%s%c%d", sign(x)?"-":"", type(x)==gtype_Inp?'i':'a', index(gate(x)));
}

void Minisat::printGate(Gate g){ printSig(mkSig(g)); }

void Minisat::printSigs(const vec<Sig>& xs)
{
    printf("{ ");
    if (xs.size() > 0)
        printSig(xs[0]);
    for (int i = 1; i < xs.size(); i++){
        printf(", ");
        printSig(xs[i]);
    }
    printf(" }");
}


