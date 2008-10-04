/*****************************************************************************************[Circ.cc]
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

#include "mtl/Sort.h"
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
    n_fanouts.growTo(tmp_gate, 0);
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
    n_fanouts.growTo(tmp_gate, 0);
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
    n_fanouts.growTo(tmp_gate, 0);
    restrashAll();
    gates[tmp_gate].strash_next = gate_Undef;
}


void Circ::push()  { gate_lim.push(gates.size()); }
void Circ::commit(){ gate_lim.pop(); }
void Circ::pop()
{
    assert(gate_lim.size() > 0);
    while ((uint32_t)gates.size() > gate_lim.last()){
        Gate g = lastGate();
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
#if 1
    static const unsigned int nprimes   = 47;
    static const unsigned int primes [] = { 31, 47, 71, 107, 163, 251, 379, 569, 853, 1279, 1931, 2897, 4349, 6529, 9803, 14713, 22073, 33113, 49669, 74507, 111767, 167663, 251501, 377257, 565889, 848839, 1273267, 1909907, 2864867, 4297301, 6445951, 9668933, 14503417, 21755137, 32632727, 48949091, 73423639, 110135461, 165203191, 247804789, 371707213, 557560837, 836341273, 1254511933, 1881767929, 2822651917U, 4233977921U };
#else
    static const unsigned int nprimes   = 26;
    static const unsigned int primes [] = {
          53
        , 97
        , 193
        , 389
        , 769
        , 1543
        , 3079
        , 6151
        , 12289
        , 24593
        , 49157
        , 98317
        , 196613
        , 393241
        , 786433
        , 1572869
        , 3145739
        , 6291469
        , 12582917
        , 25165843
        , 50331653
        , 100663319
        , 201326611
        , 402653189
        , 805306457
        , 1610612741
    };

#endif

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


void Circ::dump()
{
    for (Gate g = firstGate(); g != gate_Undef; g = nextGate(g)){
        if (type(g) == gtype_And){
            Sig x = gates[g].x;
            Sig y = gates[g].y;
        
            printf("gate %d := %s%d & %s%d\n", index(g), sign(x)?"-":"", index(gate(x)), sign(y)?"-":"", index(gate(y)));
        }else{
           printf("gate %d := <input>\n", index(g));
        }
    }   
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
        if (g == gate_True)
            copy_map[g] = sig_True;
        else if (type(g) == gtype_Inp)
            copy_map[g] = dst.mkInp();
        else{
            assert(type(g) == gtype_And);
            copy_map[g] = dst.mkAnd(_copySig(src, dst, src.lchild(g), copy_map), 
                                    _copySig(src, dst, src.rchild(g), copy_map));
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


void Minisat::copyCirc(const Circ& src, Circ& dst, GMap<Sig>& map)
{
    map.growTo(src.lastGate(), sig_Undef);

    map[gate_True] = sig_True;
    for (Gate g = src.firstGate(); g != gate_Undef; g = src.nextGate(g))
        if (map[g] == sig_Undef)
            if (type(g) == gtype_Inp)
                map[g] = dst.mkInp();
            else {
                assert(type(g) == gtype_And);
                
                Sig ix = src.lchild(g);
                Sig iy = src.rchild(g);
                Sig ux = map[gate(ix)] ^ sign(ix);
                Sig uy = map[gate(iy)] ^ sign(iy);

                map[g] = dst.mkAnd(ux, uy);
            }
}


void Minisat::copyCircWithSubst(const Circ& src, Circ& dst, GMap<Sig>& subst_map, GMap<Sig>& copy_map)
{
    // printf(" >>> COPYING CIRCUIT WITH SUBST: size-before=%d\n", dst.size());
    subst_map.growTo(src.lastGate(), sig_Undef);
    copy_map .growTo(src.lastGate(), sig_Undef);

#if 0
    printf(" >>> SUBST MAP:\n");
    for (Gate g = src.firstGate(); g != gate_Undef; g = src.nextGate(g))
        if (subst_map[g] != sig_Undef){
            Sig to = subst_map[g];
            printf("%c%d -> %s%c%d\n", 
                   type(g)==gtype_Inp?'$':'@', index(g),
                   sign(to)?"-":"", type(to)==gtype_Inp?'$':'@', index(gate(to))
                   );
        }
#endif

    copy_map[gate_True] = sig_True;
    for (Gate g = src.firstGate(); g != gate_Undef; g = src.nextGate(g))
        if (copy_map[g] == sig_Undef)
            if (type(g) == gtype_Inp)
                copy_map[g] = dst.mkInp();
            else {
                assert(type(g) == gtype_And);
                
                Sig orig_x  = src.lchild(g);
                Sig orig_y  = src.rchild(g);
                Sig subst_x = subst_map[gate(orig_x)] == sig_Undef ? orig_x : subst_map[gate(orig_x)] ^ sign(orig_x);
                Sig subst_y = subst_map[gate(orig_y)] == sig_Undef ? orig_y : subst_map[gate(orig_y)] ^ sign(orig_y);
                Sig copy_x  = copy_map[gate(subst_x)] ^ sign(subst_x);
                Sig copy_y  = copy_map[gate(subst_y)] ^ sign(subst_y);

#if 0
                printf(" >>> COPYING GATE %d:\n", index(g));
                printf(" --- orig : %s%c%d, %s%c%d\n", 
                       sign(orig_x)?"-":"", type(orig_x)==gtype_Inp?'$':'@', index(gate(orig_x)), 
                       sign(orig_y)?"-":"", type(orig_y)==gtype_Inp?'$':'@', index(gate(orig_y))
                       );
                printf(" --- subst: %s%c%d, %s%c%d\n", 
                       sign(subst_x)?"-":"", type(subst_x)==gtype_Inp?'$':'@', index(gate(subst_x)), 
                       sign(subst_y)?"-":"", type(subst_y)==gtype_Inp?'$':'@', index(gate(subst_y))
                       );
                printf(" --- copy : %s%c%d, %s%c%d\n", 
                       sign(copy_x)?"-":"", type(copy_x)==gtype_Inp?'$':'@', index(gate(copy_x)), 
                       sign(copy_y)?"-":"", type(copy_y)==gtype_Inp?'$':'@', index(gate(copy_y))
                       );
#endif
                copy_map[g] = dst.mkAnd(copy_x, copy_y);
            }
    // printf(" >>> COPYING CIRCUIT WITH SUBST: size-after=%d\n", dst.size());

#if 0
    printf(" >>> COPY MAP:\n");
    for (Gate g = src.firstGate(); g != gate_Undef; g = src.nextGate(g))
        if (copy_map[g] != sig_Undef){
            Sig to = copy_map[g];
            printf("%c%d -> %s%c%d\n", 
                   type(g)==gtype_Inp?'$':'@', index(g),
                   sign(to)?"-":"", type(to)==gtype_Inp?'$':'@', index(gate(to))
                   );
        }
#endif
}

//=================================================================================================
// Utilities for managing equivalences:
//


void Minisat::normalizeEqs(Eqs& eqs)
{
    for (int k = 0; k < eqs.size(); k++){
        vec<Sig>& cls = eqs[k];

        sort(cls);
        int i,j;
        for (i = j = 1; i < cls.size(); i++)
            if (cls[i] != cls[i-1])
                cls[j++] = cls[i];
        cls.shrink(i - j);
    }
}


void Minisat::removeTrivialEqs(Eqs& eqs)
{
    int i, j;

    for (i = j = 0; i < eqs.size(); i++)
        if (eqs[i].size() > 1){
            if (i != j) eqs[i].moveTo(eqs[j]);
            j++;
        }
    eqs.shrink(i - j);
}


void Minisat::makeSubstMap(const Circ& c, const Eqs& eqs, GMap<Sig>& m)
{
    // Initialize to identity substitution:
    m.clear();
    m.growTo(c.lastGate(), sig_Undef);
    m[gate_True] = sig_True;
    for (Gate g = c.firstGate(); g != gate_Undef; g = c.nextGate(g))
        m[g] = mkSig(g);

    // printf(" ::: MAKE SUBST MAP:\n");

    // Restrict with given equivalence:
    for (int i = 0; i < eqs.size(); i++){
        // All classes must have some element:
        assert(eqs[i].size() > 0);

        // Find minimum element:
        //
        Sig min = eqs[i][0];
        for (int j = 1; j < eqs[i].size(); j++)
            if (eqs[i][j] < min)
                min = eqs[i][j];

        // printf("CLASS %d: min=%s%c%d\n", i,
        //        sign(min)?"-":"", type(min)==gtype_Inp?'$':'@', index(gate(min))
        //        );

        // Bind all non-minimal elements of the class to the minimal element:
        //
        for (int j = 0; j < eqs[i].size(); j++)
            if (eqs[i][j] != min){
                // Gate from = gate(eqs[i][j]);
                // Sig to = min ^ sign(eqs[i][j]);
                // printf("%c%d -> %s%c%d\n", 
                //        type(from)==gtype_Inp?'$':'@', index(from),
                //        sign(to)?"-":"", type(to)==gtype_Inp?'$':'@', index(gate(to))
                //        );

                m[gate(eqs[i][j])] = min ^ sign(eqs[i][j]);
            }
    }
}
