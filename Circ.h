/******************************************************************************************[Circ.h]
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

#ifndef Circ_h
#define Circ_h

#include "CircTypes.h"
#include "SolverTypes.h"

#include <cstdio>

//=================================================================================================
// Circ -- a class for representing combinational circuits.


const uint32_t pair_hash_prime = 1073741789;

class Circ
{
    // Types:
    struct GateData { Gate strash_next; Sig x, y; };
    typedef GMap<GateData> Gates;

    struct Eq { 
        const Gates& gs; 
        Eq(const Gates& gs_) : gs(gs_) {}
        bool operator()(Gate x, Gate y) const { 
            assert(type(x) == gtype_And);
            assert(type(y) == gtype_And);
            //printf("checking %d : %c%d, %c%d\n", index(x), sign(gs[x].x)?' ':'-', index(gate(gs[x].x)), sign(gs[x].y)?' ':'-', index(gate(gs[x].y)));
            //printf("    ---- %d : %c%d, %c%d\n", index(y), sign(gs[y].x)?' ':'-', index(gate(gs[y].x)), sign(gs[y].y)?' ':'-', index(gate(gs[y].y)));
            return gs[x].x == gs[y].x && gs[x].y == gs[y].y; }
    };

    struct Hash { 
        const Gates& gs; 
        Hash(const Gates& gs_) : gs(gs_) {}
        uint32_t operator()(Gate x) const { 
            assert(type(x) == gtype_And);
            return index(gs[x].x) * pair_hash_prime + index(gs[x].y); }
    };

    // Member variables:
    Gates               gates;
    unsigned int        next_id;
    vec<unsigned int>   free_ids;
    Gate                tmp_gate;
    GMap<char>          deleted;

    Hash                gate_hash;
    Eq                  gate_eq;
    unsigned int        n_inps;
    unsigned int        n_ands;
    Gate*               strash;
    unsigned int        strash_cap;
    
    // Private methods:
    unsigned int allocId();
    void         freeId (unsigned int id);
    GateType     idType (unsigned int id) const { return gates[mkGate(id, gtype_And)].x == sig_Undef ? gtype_Inp : gtype_And; }

    void         strashInsert(Gate g);
    Gate         strashFind  (Gate g);
    void         restrashAll ();

 public:
    Circ();

    void clear ();

    int  size  () const { return gates.size()-1; }
    int  nGates() const { return n_ands; }
    int  nInps () const { return n_inps; }

    // Gate iterator:
    Gate nextGate (Gate g) const { 
        if (g == gate_Undef) return gate_Undef;
        assert(index(g) < (unsigned int)gates.size());
        do g = mkGate(index(g)+1, idType(index(g)+1)); 
        while ((int)index(g) < gates.size() && deleted[g]);
        return (index(g) == (unsigned)gates.size() || deleted[g]) ? gate_Undef : g; }
    Gate firstGate()       const { return nextGate(mkGate(0, idType(0))); }
    Gate maxGate  ()       const { return mkGate(gates.size()-1, gtype_Inp); }

    Gate gateFromId (unsigned int id) const { return mkGate(id, idType(id)); }


    // Node constructor functions:
    Sig mkInp    ();
    Sig mkAnd    (Sig x, Sig y);
    Sig mkOr     (Sig x, Sig y);
    Sig mkXorOdd (Sig x, Sig y);
    Sig mkXorEven(Sig x, Sig y);
    Sig mkXor    (Sig x, Sig y);
    
    // De-allocation:
    void freeGate(Gate g); 

    // Node inspection functions:
    Sig lchild(Gate g) const;
    Sig rchild(Gate g) const;
    Sig lchild(Sig x)  const;
    Sig rchild(Sig x)  const;
};


//=================================================================================================
// A simple container for circuit bindings, i.e. variable/definition pairs:

struct Def {
    Sig var; // type must be gtype_Inp;
    Sig def;
};

//=================================================================================================
// Circ utility functions:


// Given certain values for inputs, calculate the values of all gates in the cone of influence
// of a signal:
bool evaluate(const Circ& c, Sig x, GMap<lbool>& values);


void bottomUpOrder(Circ& c, Gate g, GSet& gset);
void bottomUpOrder(Circ& c, Sig  x, GSet& gset);
void bottomUpOrder(Circ& c, const vec<Gate>& gs, GSet& gset);
void bottomUpOrder(Circ& c, const vec<Sig>&  xs, GSet& gset);
void bottomUpOrder(Circ& c, const vec<Def>& latch_defs, GSet& gset);


Sig  copyGate(const Circ& src, Circ& dst, Gate g,      GMap<Sig>& copy_map);
Sig  copySig (const Circ& src, Circ& dst, Sig  x,      GMap<Sig>& copy_map);
void copySig (const Circ& src, Circ& dst, const vec<Sig>& xs, GMap<Sig>& copy_map);

void unitPropagate(Circ& c, const vec<Sig>& top_conjunction, GMap<Sig>& prop_map);

//=================================================================================================
// Implementation of inline methods:

inline unsigned int Circ::allocId()
{
    unsigned int id;
    if (free_ids.size() > 0){
        // There are recyclable indices:
        id = free_ids.last();
        free_ids.pop();
    }else{
        // Must choose a new index, and adjust map-size of 'gates':
        id = next_id++;
        gates.  growTo(mkGate(id, /* doesn't matter which type */ gtype_Inp));
        deleted.growTo(mkGate(id, /* doesn't matter which type */ gtype_Inp), 0);
    }

    return id;
}
inline void Circ::freeId(unsigned int id){ free_ids.push(id); }
inline void Circ::freeGate(Gate g){ 
    if (type(g) == gtype_Inp) n_inps--; else n_ands--;
    deleted.growTo(g, 0);
    deleted[g] = 1;
    freeId(index(g));
}

//=================================================================================================
// Implementation of strash-functions:


inline Gate Circ::strashFind  (Gate g)
{
    assert(type(g) == gtype_And);
    Gate h;
    //printf("searching for gate:\n");
    for (h = strash[gate_hash(g) % strash_cap]; h != gate_Undef && !gate_eq(h, g); h = gates[h].strash_next){
        //printf(" --- inspecting gate: %d\n", index(h));
        assert(type(h) == gtype_And);
    }
    return h;
}


inline void Circ::strashInsert(Gate g)
{
    assert(type(g) == gtype_And);
    assert(g != tmp_gate);
    assert(strashFind(g) == gate_Undef);
    uint32_t pos = gate_hash(g) % strash_cap;
    assert(strash[pos] != g);
    gates[g].strash_next = strash[pos];
    strash[pos] = g;
}




inline Sig  Circ::lchild(Gate g) const   { assert(type(g) == gtype_And); return gates[g].x; }
inline Sig  Circ::rchild(Gate g) const   { assert(type(g) == gtype_And); return gates[g].y; }
inline Sig  Circ::lchild(Sig  x) const   { assert(type(x) == gtype_And); return gates[gate(x)].x; }
inline Sig  Circ::rchild(Sig  x) const   { assert(type(x) == gtype_And); return gates[gate(x)].y; }
inline Sig  Circ::mkInp    ()            { n_inps++; Gate g = mkGate(allocId(), gtype_Inp); gates[g].x = sig_Undef; return mkSig(g, false); }
inline Sig  Circ::mkOr     (Sig x, Sig y){ return ~mkAnd(~x, ~y); }
inline Sig  Circ::mkXorOdd (Sig x, Sig y){ return mkOr (mkAnd(x, ~y), mkAnd(~x, y)); }
inline Sig  Circ::mkXorEven(Sig x, Sig y){ return mkAnd(mkOr(~x, ~y), mkOr ( x, y)); }
inline Sig  Circ::mkXor    (Sig x, Sig y){ return mkXorEven(x, y); }

inline Sig  Circ::mkAnd (Sig x, Sig y){
    // Simplify:
    if      (x == sig_True)  return y;
    else if (y == sig_True)  return x;
    else if (x == y)         return x;
    else if (x == sig_False || y == sig_False || x == ~y) 
        return sig_False;

    // Order:
    if (y < x) { Sig tmp = x; x = y; y = tmp; }

    // Strash-lookup:
    Gate g = tmp_gate;
    gates[g].x = x;
    gates[g].y = y;

    // fprintf(stderr, "looking up node: %c%d & %c%d\n", sign(x)?'~':' ', index(gate(x)), sign(y)?'~':' ', index(gate(y)));

    g = strashFind(g);
    if (g == gate_Undef){
        // New node needs to be created:
        g = mkGate(allocId(), gtype_And);
        gates[g].x = x;
        gates[g].y = y;
        n_ands++;

        if (n_ands > strash_cap / 2)
            restrashAll();
        else
            strashInsert(g);

        // fprintf(stderr, "created node %3d = %c%d & %c%d\n", index(g), sign(x)?'~':' ', index(gate(x)), sign(y)?'~':' ', index(gate(y)));
        //printf(" -- created new node.\n");
    }
    //else
    //    printf(" -- found old node.\n");

    return mkSig(g);
}

//=================================================================================================
// Debug etc:


//=================================================================================================
#endif
