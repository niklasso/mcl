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

#include "minisat/mtl/Sort.h"
#include "minisat/mtl/XAlloc.h"
#include "minisat/utils/Options.h"
#include "mcl/Circ.h"

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


Circ::~Circ()
{
    if (strash) free(strash);
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
    strash = (Gate*)xrealloc(strash, sizeof(Gate) * strash_cap);
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
    for (GateIt git = c.begin(); git != c.end(); ++git)
        m[*git] = mkSig(*git);

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
