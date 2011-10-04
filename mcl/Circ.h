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

#include <stdio.h>

#include "minisat/mtl/Queue.h"
#include "minisat/core/SolverTypes.h"
#include "mcl/CircTypes.h"
#include "mcl/Normalization.h"

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
    ~Circ();

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
 private:
    Gate nextGate (Gate g) const { assert(g != gate_Undef); uint32_t ind = index(g) + 1; return ind == (uint32_t)gates.size() ? gate_Undef : gateFromId(ind); }
    Gate firstGate()       const { return nextGate(gateFromId(0)); }
 public:
    Gate lastGate ()       const { return gateFromId(gates.size()-1); }

    class GateIt {
    protected:
        const Circ& c;
        Gate        g;
    public:
        GateIt(const Circ& _c, Gate _g) : c(_c), g(_g){}

        Gate   operator* () const { return g; }
        GateIt operator++()       { g = c.nextGate(g); return *this; }

        bool   operator==(const GateIt& gi) const { assert(&c == &gi.c); return g == gi.g; }
        bool   operator!=(const GateIt& gi) const { assert(&c == &gi.c); return g != gi.g; }
    };

    class InpIt : public GateIt {
    protected:
        void nextInput(){
            while (g != gate_Undef && type(g) != gtype_Inp)
                g = c.nextGate(g);
        }
    public:
        InpIt(const Circ& _c, Gate _g) : GateIt(_c,_g) { nextInput(); }
        InpIt operator++() { g = c.nextGate(g); nextInput(); return *this; }
    };

    GateIt begin0  () const { return GateIt(*this, gateFromId(0)); }
    GateIt begin   () const { return ++begin0(); }
    GateIt end     () const { return GateIt(*this, gate_Undef); }

    InpIt  inpBegin() const { return InpIt(*this, gateFromId(0)); }
    InpIt  inpEnd  () const { return InpIt(*this, gate_Undef); }

    // Node constructor functions:
    Sig mkInp    (uint32_t num = UINT32_MAX);
    Sig mkAnd    (Sig x, Sig y, bool try_only = false);
    Sig mkOr     (Sig x, Sig y);
    Sig mkXorOdd (Sig x, Sig y);
    Sig mkXorEven(Sig x, Sig y);
    Sig mkXor    (Sig x, Sig y);
    Sig mkMuxOdd (Sig x, Sig y, Sig z);
    Sig mkMuxEven(Sig x, Sig y, Sig z);
    Sig mkMux    (Sig x, Sig y, Sig z);

    // Input numbering:
    const uint32_t& number(Gate g) const { assert(type(g) == gtype_Inp); return gates[g].y.x; }
    uint32_t&       number(Gate g)       { assert(type(g) == gtype_Inp); return gates[g].y.x; }

    // Node inspection functions:
    Sig lchild(Gate g) const;
    Sig rchild(Gate g) const;
    Sig lchild(Sig x)  const;
    Sig rchild(Sig x)  const;

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


// Convenience Type Aliases for iterators:

typedef Circ::GateIt GateIt;
typedef Circ::InpIt  InpIt;

//=================================================================================================
// Box -- a class for storing inputs and outputs (roots/sinks) to a (sub) circuit:

struct Box {
    vec<Gate> inps;
    vec<Sig>  outs;

    void clear () { inps.clear(); outs.clear(); }
    void moveTo(Box& to){ inps.moveTo(to.inps); outs.moveTo(to.outs); }
    void copyTo(Box& to){ inps.copyTo(to.inps); outs.copyTo(to.outs); }
};

// NOTE: This is about to be deleted ...
#if 0

//=================================================================================================
// Flops -- a class for representing the sequential behaviour of a circuit:

class Flops {
    vec<Gate>  gates;
    GMap<Sig>  defs;
    SMap<char> is_def; // is_def[s] is true iff there exists a gate g such that defs[g] == s

 public:
    void clear()               { gates.clear(); defs.clear(); is_def.clear(); }
    void defineFlop(Gate f, Sig def){
        assert(type(f) == gtype_Inp);

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

    // FIXME: the following methods don't need to be member functions, but could instead be defined
    // separately.

    // Duplicate the set of flops such that for each flop 'f', with input signal 'def', the result
    // contains a corresponding flop where the input signal has been remapped according to 'm':
    void mapInps(const GMap<Sig>& m, Flops& to) const {
        to.clear();
        for (int i = 0; i < gates.size(); i++){
            Gate f   = gates[i];
            Sig  def = defs[f];
            assert(m[gate(def)] != sig_Undef);
            to.defineFlop(f, m[gate(def)] ^ sign(def));
        }
    }

    // Duplicate the set of flops such that for each flop with output gate 'f', the result contains
    // a corresponding flop where the output gate has been remapped according to 'm'.
    // PRECONDITION: the gate-map 'm' can not map a gate to a signed signal so that the result can
    // be used as a new flop output.
    void mapOuts(const GMap<Sig>& m, Flops& to) const {
        to.clear();
        for (int i = 0; i < gates.size(); i++){
            Gate f   = gates[i];
            Sig  def = defs[f];
            assert(m[f] != sig_Undef);
            assert(!sign(m[f]));
            to.defineFlop(gate(m[f]), def);
        }
    }

    // Performs a simulatenous 'mapInps()' and 'mapOuts()', completely adjusting a set of flops to
    // reflect the change from a map 'm':
    void map(const GMap<Sig>& m, Flops& to) const {
        to.clear();
        for (int i = 0; i < gates.size(); i++){
            Gate f   = gates[i];
            Sig  def = defs[f];
            assert(m[f] != sig_Undef);
            assert(m[gate(def)] != sig_Undef);
            assert(!sign(m[f]));
            to.defineFlop(gate(m[f]), m[gate(def)] ^ sign(def));
        }
    }

};

static inline
void copy    (const Flops& from, Flops& to){ from.copyTo(to); }

static inline void extractSigs(const Flops& flps, vec<Sig>& xs){
     for (int i = 0; i < flps.size(); i++){
         xs.push(mkSig(flps[i]));
         xs.push(flps.def(flps[i]));
     } }

static inline void mapInps(const GMap<Sig>& m, Flops& flps){ Flops tmp; flps.mapInps(m, tmp); tmp.moveTo(flps); }
static inline void mapOuts(const GMap<Sig>& m, Flops& flps){ Flops tmp; flps.mapOuts(m, tmp); tmp.moveTo(flps); }
static inline void map    (const GMap<Sig>& m, Flops& flps){ Flops tmp; flps.map(m, tmp);     tmp.moveTo(flps); }


#endif

//=================================================================================================
// Circ utility functions:

// FIXME: clean up this utility section. Maybe move to a separate "Circuit Prelude" source file?

// FIXME: Move to CircTypes.
typedef vec<vec<Sig> > Eqs;

void normalizeEqs(Eqs& eqs);
void removeTrivialEqs(Eqs& eqs);
void makeSubstMap(const Circ& c, const Eqs& eqs, GMap<Sig>& m);

// Extract signals contained within some data-structure:
static inline void extractSigs(Sig x,  vec<Sig>& xs){ xs.push(x); }
static inline void extractSigs(Gate g, vec<Sig>& xs){ xs.push(mkSig(g)); }
template <class T>
static inline void extractSigs(const vec<T>& ys, vec<Sig>& xs){
    for (int i = 0; i < ys.size(); i++)
        extractSigs(ys[i], xs); }
static inline void extractSigs(const Box& b, vec<Sig>& xs){ extractSigs(b.inps, xs); extractSigs(b.outs, xs); }
//=================================================================================================
// Map functions:
//


static inline void map(const GMap<Sig>& m, Gate& g)    { if (g != gate_Undef) g = gate(m[g]); } // Use with care!
static inline void map(const GMap<Sig>& m, Sig&  x){ 
    if (x != sig_Undef){
        Sig y = m[gate(x)];
        if (y == sig_Undef)
            x = sig_Undef;
        else
            x = y ^ sign(x);
    }
}
static inline void map(const GMap<Sig>& m, Box& b){
    for (int i = 0; i < b.inps.size(); i++){ assert(!sign(m[b.inps[i]])); b.inps[i] = gate(m[b.inps[i]]); }
    for (int i = 0; i < b.outs.size(); i++) b.outs[i] = m[gate(b.outs[i])] ^ sign(b.outs[i]);
}


template<class T>
static inline void map(const GMap<Sig>& m, vec<T>& xs){
    for (int i = 0; i < xs.size(); i++)
        map(m, xs[i]);
}

static inline
void map(const GMap<Sig>& m, GMap<Sig>& tm){
    GMap<Sig>::iterator it;
    for (it = tm.begin(); it != tm.end(); ++it)
        if (m.has(gate(*it)))
            map(m, *it);
        else
            *it = sig_Undef;
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
inline Sig  Circ::mkOr     (Sig x, Sig y){ return ~mkAnd(~x, ~y); }
inline Sig  Circ::mkXorOdd (Sig x, Sig y){ return mkOr (mkAnd(x, ~y), mkAnd(~x, y)); }
inline Sig  Circ::mkXorEven(Sig x, Sig y){ return mkAnd(mkOr(~x, ~y), mkOr ( x, y)); }
inline Sig  Circ::mkXor    (Sig x, Sig y){ return mkXorEven(x, y); }
inline Sig  Circ::mkMuxOdd (Sig x, Sig y, Sig z) { return mkOr (mkAnd( x, y), mkAnd(~x, z)); }
inline Sig  Circ::mkMuxEven(Sig x, Sig y, Sig z) { return mkAnd(mkOr (~x, y), mkOr ( x, z)); }
inline Sig  Circ::mkMux    (Sig x, Sig y, Sig z) { return mkMuxEven(x, y, z); }
inline Sig  Circ::mkInp    (uint32_t num){ 
    n_inps++; 
    Gate g = mkGate(allocId(), gtype_Inp); 
    gates[g].x   = sig_Undef; 
    gates[g].y.x = num;
    return mkSig(g, false); }

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
