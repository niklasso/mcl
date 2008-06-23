/******************************************************************************************[Circ.h]
Copyright (c) 2007-2008, Niklas Sorensson

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

#ifndef Minisat_Circ_h
#define Minisat_Circ_h

#include "mtl/Queue.h"
#include "core/SolverTypes.h"
#include "circ/CircTypes.h"
#include "circ/Matching.h"
#include "circ/Normalization.h"

#include <cstdio>

namespace Minisat {

//=================================================================================================
// Circ -- a class for representing combinational circuits.


static const uint32_t pair_hash_prime = 1073741789;

class Circ
{
    // Types:
    struct GateData { Gate strash_next; Sig x, y; };
    typedef GMap<GateData> Gates;

    // Member variables:
    //
    Gates               gates;        // Gates[0] is reserved for the constant gate_True.
    GMap<uint8_t>       n_fanouts;

    unsigned int        n_inps;
    unsigned int        n_ands;
    Gate*               strash;
    unsigned int        strash_cap;
    vec<uint32_t>       gate_lim;

    Gate                tmp_gate;

    GSet                tmp_set;
    vec<Sig>            tmp_stack;
    GMap<int>           tmp_fanouts;
    
    // Private methods:
    //
    unsigned int allocId     ();
    GateType     idType      (unsigned int id) const;
    uint32_t     gateHash    (Gate g)          const;
    bool         gateEq      (Gate x, Gate y)  const;
    void         strashInsert(Gate g);
    Gate         strashFind  (Gate g)          const;
    void         strashRemove(Gate g);
    void         restrashAll ();

    Gate         gateFromId  (unsigned int id) const;

 public:
    // Mode of operation:
    //
    uint32_t rewrite_mode;

    Circ();

    // Misc. :
    //
    int  size  () const { return gates.size()-1; }
    int  nGates() const { return n_ands; }
    int  nInps () const { return n_inps; }
    int  nFanouts  (Gate g) const { return n_fanouts[g]; }
    void bumpFanout(Gate g) { n_fanouts[g]++; }

    // Environment state manipulation:
    //
    void clear  ();
    void moveTo (Circ& to);

    void push   ();
    void pop    ();
    void commit ();

    // Gate iterator:
    Gate nextGate (Gate g) const { assert(g != gate_Undef); uint32_t ind = index(g) + 1; return ind == (uint32_t)gates.size() ? gate_Undef : gateFromId(ind); }
    Gate firstGate()       const { return nextGate(gateFromId(0)); }
    Gate lastGate ()       const { return gateFromId(gates.size()-1); }

    // Node constructor functions:
    Sig mkInp    ();
    Sig mkAnd    (Sig x, Sig y, bool try_only = false);
    Sig mkOr     (Sig x, Sig y);
    Sig mkXorOdd (Sig x, Sig y);
    Sig mkXorEven(Sig x, Sig y);
    Sig mkXor    (Sig x, Sig y);
    Sig mkMuxOdd (Sig x, Sig y, Sig z);
    Sig mkMuxEven(Sig x, Sig y, Sig z);
    Sig mkMux    (Sig x, Sig y, Sig z);
    
    // Node inspection functions:
    Sig lchild(Gate g) const;
    Sig rchild(Gate g) const;
    Sig lchild(Sig x)  const;
    Sig rchild(Sig x)  const;

    // Pattern matching functions:
    bool matchMuxParts(Gate g, Gate h, Sig& x, Sig& y, Sig& z);
    bool matchMux (Gate g, Sig& x, Sig& y, Sig& z);
    bool matchXor (Gate g, Sig& x, Sig& y);
    bool matchXors(Gate g, vec<Sig>& xs);
    void matchAnds(Gate g, vec<Sig>& xs, bool match_muxes = false);
    void matchTwoLevel
                  (Gate g, vec<vec<Sig> >& xss, bool match_muxes = false);

    // Lookup whether different different patterns already exists somewhere:
    Sig tryAnd     (Sig x, Sig y);
    int costAnd    (Sig x, Sig y);
    int costXorOdd (Sig x, Sig y);
    int costXorEven(Sig x, Sig y);
    int costMuxOdd (Sig x, Sig y, Sig z);
    int costMuxEven(Sig x, Sig y, Sig z);

    // Debug
    void dump();
};


//=================================================================================================
// Box -- a class for storing inputs and outputs (roots/sinks) to a (sub) circuit:

struct Box {
    vec<Gate> inps;
    vec<Sig>  outs;

    void clear () { inps.clear(); outs.clear(); }
    void moveTo(Box& to){ inps.moveTo(to.inps); outs.moveTo(to.outs); }
    void copyTo(Box& to){ inps.copyTo(to.inps); outs.copyTo(to.outs); }
    void remap (const GMap<Sig>& map, Box& to){
        to.clear();
        for (int i = 0; i < inps.size(); i++) to.inps.push(gate(map[inps[i]]));
        for (int i = 0; i < outs.size(); i++) to.outs.push(map[gate(outs[i])] ^ sign(outs[i]));
    }
};

//=================================================================================================
// Flops -- a class for representing the sequential behaviour of a circuit:

class Flops {
    vec<Gate>  gates;
    GMap<Sig>  defs;
    SMap<char> is_def; // is_def[s] is true iff there exists a gate g such that defs[g] == s

 public:
    void clear()               { gates.clear(); defs.clear(); is_def.clear(); }
    void defineFlop(Gate f, Sig def){
        defs  .growTo(f,   sig_Undef);
        is_def.growTo(def, 0);

        gates.push(f);
        defs  [f]   = def;
        is_def[def] = 1; }

    bool isDef     (Sig x) const { return is_def.has(x) && is_def[x]; }
    bool isFlop    (Gate f)const { return defs.has(f) && (defs[f] != sig_Undef); }
    Sig  def       (Gate f)const { return defs[f]; }
    int  size      ()      const { return gates.size(); }
    Gate operator[](int i) const { return gates[i]; }

    void moveTo(Flops& to){ gates.moveTo(to.gates); defs.moveTo(to.defs); is_def.moveTo(to.is_def); }
    void copyTo(Flops& to) const { gates.copyTo(to.gates); defs.copyTo(to.defs); is_def.copyTo(to.is_def); }
    void remap (const Circ& c, const GMap<Sig>& map, Flops& to) const {
        remap(map, to); }

    void remap (const GMap<Sig>& map, Flops& to) const {
        to.clear();
        for (int i = 0; i < gates.size(); i++){
            Gate f   = gates[i];
            Sig  def = defs[f];
            assert(map[f] != sig_Undef);
            assert(map[gate(def)] != sig_Undef);
            assert(!sign(map[f]));
            to.defineFlop(gate(map[f]), map[gate(def)] ^ sign(def));
        }
    }
};


//=================================================================================================
// Circ utility functions:


// Given certain values for inputs, calculate the values of all gates in the cone of influence
// of a signal:
bool evaluate(const Circ& c, Sig x, GMap<lbool>& values);

void bottomUpOrder(const Circ& c, Gate g, GSet& gset);
void bottomUpOrder(const Circ& c, Sig  x, GSet& gset);
void bottomUpOrder(const Circ& c, const vec<Gate>& gs, GSet& gset);
void bottomUpOrder(const Circ& c, const vec<Sig>&  xs, GSet& gset);
void bottomUpOrder(const Circ& c, const vec<Gate>& latches, const GMap<Sig>& latch_defs, GSet& gset);

Sig  copyGate(const Circ& src, Circ& dst, Gate g,      GMap<Sig>& copy_map);
Sig  copySig (const Circ& src, Circ& dst, Sig  x,      GMap<Sig>& copy_map);
void copySig (const Circ& src, Circ& dst, const vec<Sig>& xs, GMap<Sig>& copy_map);
void copyCirc(const Circ& src, Circ& dst, GMap<Sig>& map);

void circInfo(      Circ& c, Gate g, GSet& reachable, int& n_ands, int& n_xors, int& n_muxes, int& tot_ands);

 static inline
 void copy    (const Flops& from, Flops& to){ from.copyTo(to); }
 static inline
 void remap   (const GMap<Sig>& map, Flops& flps){ 
     Flops tmp;
     flps.remap(map, tmp);
     tmp.moveTo(flps);
 }

 static inline
 void remap   (const GMap<Sig>& map, Gate& g) { g = gate(map[g]); } // Use with care!
 static inline
 void remap   (const GMap<Sig>& map, Sig&  x) { x = map[gate(x)] ^ sign(x); }

 template<class T>
 static inline
 void remap   (const GMap<Sig>& map, vec<T>& xs){
     for (int i = 0; i < xs.size(); i++)
         remap(map, xs[i]);
 }

//=================================================================================================
// Implementation of inline methods:

inline unsigned int Circ::allocId()
{
    uint32_t id = gates.size();
    Gate     g  = mkGate(id, /* doesn't matter which type */ gtype_Inp);
    gates.growTo(g);
    n_fanouts.growTo(g, 0);
    assert((uint32_t)gates.size() == id + 1);
    return id;
}


inline GateType Circ::idType(unsigned int id) const { 
    return id == 0 ? gtype_Const : gates[mkGate(id, gtype_And)].x == sig_Undef ? gtype_Inp : gtype_And; }
inline Gate     Circ::gateFromId(unsigned int id) const { return mkGate(id, idType(id)); }

inline bool Circ::matchMuxParts(Gate g, Gate h, Sig& x, Sig& y, Sig& z) { return Minisat::matchMuxParts(*this, g, h, x, y, z); }
inline bool Circ::matchMux (Gate g, Sig& x, Sig& y, Sig& z) { return Minisat::matchMux(*this, g, x, y, z); }
inline bool Circ::matchXor (Gate g, Sig& x, Sig& y) { return Minisat::matchXor(*this, g, x, y); }
inline bool Circ::matchXors(Gate g, vec<Sig>& xs) { return Minisat::matchXors(*this, g, tmp_stack, xs); }
inline void Circ::matchAnds(Gate g, vec<Sig>& xs, bool match_muxes) { Minisat::matchAnds(*this, g, tmp_set, tmp_stack, tmp_fanouts, xs, match_muxes); }
inline void Circ::matchTwoLevel(Gate g, vec<vec<Sig> >& xss, bool match_muxes) { 
    Minisat::matchTwoLevel(*this, g, tmp_set, tmp_stack, tmp_fanouts, xss, match_muxes); }

//=================================================================================================
// Implementation of strash-functions:

inline uint32_t Circ::gateHash(Gate g) const
{
    assert(type(g) == gtype_And);
    return index(gates[g].x) * pair_hash_prime + index(gates[g].y); 
}


inline bool Circ::gateEq(Gate x, Gate y) const
{
    assert(x != gate_Undef);
    assert(y != gate_Undef);
    assert(type(x) == gtype_And);
    assert(type(y) == gtype_And);
    //printf("checking %d : %c%d, %c%d\n", index(x), sign(gs[x].x)?' ':'-', index(gate(gs[x].x)), sign(gs[x].y)?' ':'-', index(gate(gs[x].y)));
    //printf("    ---- %d : %c%d, %c%d\n", index(y), sign(gs[y].x)?' ':'-', index(gate(gs[y].x)), sign(gs[y].y)?' ':'-', index(gate(gs[y].y)));
    return gates[x].x == gates[y].x && gates[x].y == gates[y].y;
}


inline Gate Circ::strashFind  (Gate g) const
{
    assert(type(g) == gtype_And);
    Gate h;
    //printf("searching for gate:\n");
    for (h = strash[gateHash(g) % strash_cap]; h != gate_Undef && !gateEq(h, g); h = gates[h].strash_next)
        //printf(" --- inspecting gate: %d\n", index(h));
        ;
    return h;
}


inline void Circ::strashRemove(Gate g)
{
    Gate* h;
    // printf("searching for gate %d to delete:\n", index(g));
    for (h = &strash[gateHash(g) % strash_cap]; !gateEq(*h, g); h = &gates[*h].strash_next)
        // printf(" --- inspecting gate: %d\n", index(*h));
        ;
    assert(*h != gate_Undef);
    assert(gateEq(*h, g));

    *h = gates[*h].strash_next;
}


inline void Circ::strashInsert(Gate g)
{
    assert(type(g) == gtype_And);
    assert(g != tmp_gate);
    assert(strashFind(g) == gate_Undef);
    uint32_t pos = gateHash(g) % strash_cap;
    assert(strash[pos] != g);
    gates[g].strash_next = strash[pos];
    strash[pos] = g;
}




inline Sig  Circ::lchild(Gate g) const   { assert(type(g) == gtype_And && g != gate_True && g != gate_Undef); return gates[g].x; }
inline Sig  Circ::rchild(Gate g) const   { assert(type(g) == gtype_And && g != gate_True && g != gate_Undef); return gates[g].y; }
inline Sig  Circ::lchild(Sig  x) const   { assert(type(x) == gtype_And && gate(x) != gate_True && x != sig_Undef); return gates[gate(x)].x; }
inline Sig  Circ::rchild(Sig  x) const   { assert(type(x) == gtype_And && gate(x) != gate_True && x != sig_Undef); return gates[gate(x)].y; }
inline Sig  Circ::mkInp    ()            { n_inps++; Gate g = mkGate(allocId(), gtype_Inp); gates[g].x = sig_Undef; return mkSig(g, false); }
inline Sig  Circ::mkOr     (Sig x, Sig y){ return ~mkAnd(~x, ~y); }
inline Sig  Circ::mkXorOdd (Sig x, Sig y){ return mkOr (mkAnd(x, ~y), mkAnd(~x, y)); }
inline Sig  Circ::mkXorEven(Sig x, Sig y){ return mkAnd(mkOr(~x, ~y), mkOr ( x, y)); }
inline Sig  Circ::mkXor    (Sig x, Sig y){ return mkXorEven(x, y); }
inline Sig  Circ::mkMuxOdd (Sig x, Sig y, Sig z) { return mkOr (mkAnd( x, y), mkAnd(~x, z)); }
inline Sig  Circ::mkMuxEven(Sig x, Sig y, Sig z) { return mkAnd(mkOr (~x, y), mkOr ( x, z)); }
inline Sig  Circ::mkMux    (Sig x, Sig y, Sig z) { return mkMuxEven(x, y, z); }

inline Sig  Circ::mkAnd    (Sig x, Sig y, bool try_only){
    assert(x != sig_Undef);
    assert(y != sig_Undef);

    // Simplify:
    if (rewrite_mode >= 1){
        if      (x == sig_True)  return y;
        else if (y == sig_True)  return x;
        else if (x == y)         return x;
        else if (x == sig_False || y == sig_False || x == ~y) 
            return sig_False;
    }


    if (rewrite_mode >= 2)
        // Repeat while productive:
        //
        for (;;){
            Sig l  = x;
            Sig r  = y;
            Sig ll = type(l) == gtype_Inp ? sig_Undef : lchild(l);
            Sig lr = type(l) == gtype_Inp ? sig_Undef : rchild(l);
            Sig rl = type(r) == gtype_Inp ? sig_Undef : lchild(r);
            Sig rr = type(r) == gtype_Inp ? sig_Undef : rchild(r);
            
        // Level two rules:
        //
            if (false)
                ;
#if 1
            else if (!sign(l) && type(l) == gtype_And &&             (ll == ~r || lr == ~r)) 
                return sig_False; // Contradiction 1.1
            else if (!sign(r) && type(r) == gtype_And &&             (rl == ~l || rr == ~l)) 
                return sig_False; // Contradiction 1.2
            else if (!sign(l) && type(l) == gtype_And && !sign(r) && type(r) == gtype_And && (ll == ~rl || ll == ~rr || lr == ~rl || lr == ~rr))
                return sig_False; // Contradiction 2
#endif

#if 1
            else if ( sign(l) && type(l) == gtype_And && (ll == ~r || lr == ~r))             
                return r;         // Subsumption 1.1
            else if ( sign(r) && type(r) == gtype_And && (rl == ~l || rr == ~l)) 
                return l;         // Subsumption 1.2
            
            else if ( sign(l) && type(l) == gtype_And && !sign(r) && type(r) == gtype_And && (ll == ~rl || ll == ~rr || lr == ~rl || lr == ~rr)) 
                return r;         // Subsumption 2.1
            else if (!sign(l) && type(l) == gtype_And &&  sign(r) && type(r) == gtype_And && (ll == ~rl || ll == ~rr || lr == ~rl || lr == ~rr)) 
                return l;         // Subsumption 2.2
#endif

#if 1
            else if (!sign(l) && type(l) == gtype_And && (ll ==  r || lr ==  r))
                return l;         // Idempotency 1.1
            else if (!sign(r) && type(r) == gtype_And && (rl ==  l || rr ==  l))             
                return r;         // Idempotency 1.2
#endif

#if 1
            else if ( sign(l) && type(l) == gtype_And &&  sign(r) && type(r) == gtype_And && ( (ll == rl && lr == ~rr) || (ll == rr && lr == ~rl) )) 
                return ~ll;       // Resolution 1.1
            else if ( sign(l) && type(l) == gtype_And &&  sign(r) && type(r) == gtype_And && ( (lr == rl && ll == ~rr) || (lr == rr && ll == ~rl) )) 
                return ~lr;       // Resolution 1.1
#endif
            
#if 1
            else if ( sign(l) && type(l) == gtype_And && (ll == r)) 
                { x = ~lr; y = r; }                        // Substitution 1.1
            else if ( sign(l) && type(l) == gtype_And && (lr == r)) 
                { x = ~ll; y = r; }                        // Substitution 1.2
            else if ( sign(r) && type(r) == gtype_And && (rl == l)) 
                { x = ~rr; y = l; }                        // Substitution 1.3
            else if ( sign(r) && type(r) == gtype_And && (rr == l))
                { x = ~rl; y = l; }                        // Substitution 1.4

            else if ( sign(l) && type(l) == gtype_And && !sign(r) && type(r) == gtype_And && (ll == rl || ll == rr)) 
                { x = ~lr; y = r; } // Substitution 2.1
            else if ( sign(l) && type(l) == gtype_And && !sign(r) && type(r) == gtype_And && (lr == rl || lr == rr)) 
                { x = ~ll; y = r; } // Substitution 2.2
            else if (!sign(l) && type(l) == gtype_And &&  sign(r) && type(r) == gtype_And && (rl == ll || rl == lr)) 
                { x = ~rr; y = l; } // Substitution 2.3
            else if (!sign(l) && type(l) == gtype_And &&  sign(r) && type(r) == gtype_And && (rr == ll || rr == lr)) 
                { x = ~rl; y = l; } // Substitution 2.4
#endif
            else
                break;
        
    }

    Gate g = gate_Undef;
    if (rewrite_mode >= 1){
        assert(x != y);
        assert(x != ~y);
        assert(x != sig_True);
        assert(x != sig_False);
        assert(y != sig_True);
        assert(y != sig_False);

        // Order:
        if (y < x) { Sig tmp = x; x = y; y = tmp; }

        // Strash-lookup:
        g = mkGate(index(tmp_gate), gtype_And);
        gates[g].x = x;
        gates[g].y = y;

        // fprintf(stderr, "looking up node: %c%d & %c%d\n", sign(x)?'~':' ', index(gate(x)), sign(y)?'~':' ', index(gate(y)));
        
        g = strashFind(g);
    }

    if (!try_only && g == gate_Undef){
        // New node needs to be created:
        g = mkGate(allocId(), gtype_And);
        gates[g].x = x;
        gates[g].y = y;
        n_ands++;
        
        // Insert into strash map:
        if (n_ands > strash_cap / 2)
            restrashAll();
        else
            strashInsert(g);

        // Update fanout counters:
        if (n_fanouts[gate(x)] < 255) n_fanouts[gate(x)]++;
        if (n_fanouts[gate(y)] < 255) n_fanouts[gate(y)]++;

        // if (n_fanouts[gate(x)] == 255) printf("High fanout detected!\n");
        // if (n_fanouts[gate(y)] == 255) printf("High fanout detected!\n");

        // fprintf(stderr, "created node %3d = %c%d & %c%d\n", index(g), sign(x)?'~':' ', index(gate(x)), sign(y)?'~':' ', index(gate(y)));
        //printf(" -- created new node.\n");
    }
    //else
    //    printf(" -- found old node.\n");

    return mkSig(g);
}

#if 0
inline Sig Circ::tryAnd(Sig x, Sig y)
{
    assert(gate(x) != gate_True);
    assert(gate(y) != gate_True);

    // Order:
    if (y < x) { Sig tmp = x; x = y; y = tmp; }

    // Strash-lookup:
    Gate g = mkGate(index(tmp_gate), gtype_And);
    gates[g].x = x;
    gates[g].y = y;

    return mkSig(strashFind(g));
}
#else
inline Sig Circ::tryAnd(Sig x, Sig y) { return mkAnd(x, y, true); }
#endif

inline int Circ::costAnd (Sig x, Sig y)
{
    return tryAnd(x, y) == sig_Undef ? 1 : 0;
}

#if 1
inline int Circ::costMuxOdd (Sig x, Sig y, Sig z)
{
    // return mkOr (mkAnd( x, y), mkAnd(~x, z));
    Sig a = tryAnd( x, y);
    Sig b = tryAnd(~x, z);
    Sig c = sig_Undef;

    if (a != sig_Undef && b != sig_Undef)
        c = tryAnd(~a, ~b);

    return (int)(a == sig_Undef) + (int)(b == sig_Undef) + (int)(c == sig_Undef);
}
#else
inline int Circ::costMuxOdd (Sig x, Sig y, Sig z)
{
    uint32_t gates_before = nGates();
    push();
    mkMuxOdd(x, y, z);
    uint32_t gates_after  = nGates();
    pop();
    return gates_after - gates_before;
}
#endif

#if 1
inline int Circ::costMuxEven(Sig x, Sig y, Sig z)
{
    // return mkAnd(mkOr (~x, y), mkOr ( x, z));

    Sig a = tryAnd( x, ~y);
    Sig b = tryAnd(~x, ~z);
    Sig c = sig_Undef;

    if (a != sig_Undef && b != sig_Undef)
        c = tryAnd(~a, ~b);

    return (int)(a == sig_Undef) + (int)(b == sig_Undef) + (int)(c == sig_Undef);
}
#else
inline int Circ::costMuxEven(Sig x, Sig y, Sig z)
{
    uint32_t gates_before = nGates();
    push();
    mkMuxEven(x, y, z);
    uint32_t gates_after  = nGates();
    pop();
    return gates_after - gates_before;
}
#endif

inline int Circ::costXorOdd (Sig x, Sig y){ return costMuxOdd (x, ~y, y); }
inline int Circ::costXorEven(Sig x, Sig y){ return costMuxEven(x, ~y, y); }


//=================================================================================================
// Debug etc:


//=================================================================================================

};

#endif
