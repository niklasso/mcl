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


const uint32_t pair_hash_prime = 148814101;

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
    
    vec<vec<Sig> > constraints;

    // Private methods:
    unsigned int allocId();
    void         freeId (unsigned int id);

    void         strashInsert(Gate g);
    Gate         strashFind  (Gate g);
    void         restrash    ();

 public:
    Circ() : next_id(1), tmp_gate(gate_True), gate_hash(Hash(gates)), gate_eq(Eq(gates)), n_inps(0), n_ands(0), strash(NULL), strash_cap(0) 
        { 
            restrash();
            gates.growTo(tmp_gate); 
            gates[tmp_gate].strash_next = gate_Undef;
        }

    int nGates() const { return n_ands; }
    int nInps () const { return n_inps; }

    // Node constructor functions:
    Sig mkInp    ();
    Sig mkAnd    (Sig x, Sig y);
    Sig mkOr     (Sig x, Sig y);
    Sig mkXorOdd (Sig x, Sig y);
    Sig mkXorEven(Sig x, Sig y);
    Sig mkXor    (Sig x, Sig y);
    
    // De-allocation:
    void freeGate(Gate g); 

    // Add extra implications:
    void constrain(const vec<Sig>& xs) { constraints.push(); xs.copyTo(constraints.last()); }
    template<class Solver>
    void addConstraints(Solver& S, GMap<Var>& vmap);
    
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
        gates.growTo(mkGate(id, /* doesn't matter which type */ gtype_Inp));
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

inline void Circ::strashInsert(Gate g)
{
    assert(g != tmp_gate);
    assert(strashFind(g) == gate_Undef);
    assert(type(g) == gtype_And);
    if (n_ands + 1 > strash_cap / 2) 
        restrash();
    else {
        uint32_t pos = gate_hash(g) % strash_cap;
        assert(strash[pos] != g);
        gates[g].strash_next = strash[pos];
        strash[pos] = g;
    }
}


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

    //printf("looking up node: %c%d & %c%d\n", sign(x)?'~':' ', index(gate(x)), sign(y)?'~':' ', index(gate(y)));

    g = strashFind(g);
    if (g == gate_Undef){
        // New node needs to be created:
        g = mkGate(allocId(), gtype_And);
        gates[g].x = x;
        gates[g].y = y;
        strashInsert(g);
        n_ands++;
        //printf("created node %3d = %c%d & %c%d\n", index(g), sign(x)?'~':' ', index(gate(x)), sign(y)?'~':' ', index(gate(y)));
        //printf(" -- created new node.\n");
    }
    //else
    //    printf(" -- found old node.\n");

    return mkSig(g);
}

template<class Solver>
void Circ::addConstraints(Solver& S, GMap<Var>& vmap)
{
    int cnt = 0;

    for (int i = 0; i < constraints.size(); i++){
        vec<Sig>& constr = constraints[i];
        vec<Lit>  clause;

        for (int j = 0; j < constr.size(); j++){
            vmap.growTo(gate(constr[j]), var_Undef);
            if (vmap[gate(constr[j])] == var_Undef)
                goto undefined;
            else
                clause.push(mkLit(vmap[gate(constr[j])], sign(constr[j])));
        }

        cnt++;
        S.addClause(clause);
        if (i+1 < constraints.size()){
            constraints.last().moveTo(constr);
            constraints.pop();
            i--;
        }

    undefined:
        ;
    }
    printf("Added %d extra constraints (%d left)\n", cnt, constraints.size());
}


//=================================================================================================
// Debug etc:


//=================================================================================================
#endif
