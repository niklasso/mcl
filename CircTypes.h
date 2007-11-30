/*************************************************************************************[CircTypes.h]
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

#ifndef CircTypes_h
#define CircTypes_h

#include "Vec.h"

//=================================================================================================
// Helper types (analogues of Var/Lit types from MiniSat):

typedef enum { gtype_Inp = 0, gtype_And = 1 } GateType;

//-------------------------------------------------------------------------------------------------
// Gate-type: (analogue of MiniSat's Var)


// --- bit 0: type { 0 = inp, 1 = and }, 1 : unused, 2-31 data ---
struct Gate {
    unsigned int x;
    bool operator == (Gate p) const { return x == p.x; }
    bool operator != (Gate p) const { return x != p.x; }
    bool operator <  (Gate p) const { return x < p.x;  }
};

// Use this as a constructor:
inline Gate         mkGate  (unsigned int id, GateType t){ Gate g; g.x = (id << 2) + t; return g; }
inline GateType     type    (Gate g) { return GateType(g.x & 1); }

const Gate gate_Undef = { ((1U << 30)-1) << 2 };
const Gate gate_True  = { 1 };

// Note! Use GMap instead of this:
inline unsigned int index (Gate g) { return g.x >> 2; }

//-------------------------------------------------------------------------------------------------
// Signal-type: (analogue of MiniSat's Lit)

// --- bit 0: type { 0 = inp, 1 = and }, 1 : sign, 2-31 data ---
struct Sig {
    unsigned int x;
    bool operator == (Sig p) const { return x == p.x; }
    bool operator != (Sig p) const { return x != p.x; }
    bool operator <  (Sig p) const { return x < p.x;  } // '<' makes p, ~p adjacent in the ordering.
};


// Use this as a constructor:
inline  Sig mkSig(Gate g, bool sign = false){ 
    Sig p; p.x = g.x + (unsigned)sign + (unsigned)sign; return p; }

inline  Sig      operator ~(Sig p)                       { Sig q; q.x = p.x ^ 2; return q; }
inline  Sig      operator ^(Sig p, bool b)               { Sig q; q.x = p.x ^ (((unsigned int)b)<<1); return q; }
inline  bool     sign      (Sig p)                       { return bool(p.x & 2); }
inline  Gate     gate      (Sig p)                       { return mkGate(p.x >> 2, GateType(p.x & 1)); }
inline  GateType type      (Sig p)                       { return GateType(p.x & 1); }

// Mapping Sigerals to and from compact integers suitable for array indexing:
inline  Sig          toSig     (unsigned  i)         { Sig p; p.x = i; return p; } 

//const Sig lit_Undef = mkSig(var_Undef, false);  // }- Useful special constants.
//const Sig lit_Error = mkSig(var_Undef, true );  // }

const Sig sig_Undef = { ((1U << 30)-1) << 2 };  // }- Useful special constants.
const Sig sig_Error = { (((1U << 30)-1) << 2) + 2};  // }
const Sig sig_True  = { 1 };
const Sig sig_False = { 3 };

// Note! Use SMap instead of this:
inline unsigned int index (Sig s) { return s.x >> 1; }


//=================================================================================================
// Map-types:

template<class T>
class GMap : private vec<T>
{
 public:
    // Vector interface:
    const T& operator [] (Gate g) const { return ((const vec<T>&)(*this))[index(g)]; }
    T&       operator [] (Gate g)       { return ((vec<T>&)(*this))      [index(g)]; }

    // Note the slightly different semantics to vec's growTo, namely that
    // this guarantees that the element 'g' can be indexed after this operation.
    void     growTo (Gate g)             { ((vec<T>&)(*this)).growTo(index(g) + 1   ); }
    void     growTo (Gate g, const T& e) { ((vec<T>&)(*this)).growTo(index(g) + 1, e); }

    void     clear  (bool free = false)  { ((vec<T>&)(*this)).clear(free); }
    int      size   () const             { return ((vec<T>&)(*this)).size(); }
};


template<class T>
class SMap : private vec<T>
{
 public:
    // Vector interface:
    const T& operator [] (Sig x) const { return ((const vec<T>&)(*this))[index(x)]; }
    T&       operator [] (Sig x)       { return ((vec<T>&)(*this))      [index(x)]; }

    // Note the slightly different semantics to vec's capacity, namely that
    // this guarantees that the element 'g' can be indexed after this operation.
    void     growTo (Sig x)             { ((vec<T>&)(*this)).growTo(index(x) + 1   ); }
    void     growTo (Sig x, const T& e) { ((vec<T>&)(*this)).growTo(index(x) + 1, e); }

    void     clear  (bool free = false)  { ((vec<T>&)(*this)).clear(free); }
    int      size   () const             { return ((vec<T>&)(*this)).size(); }
};


//=================================================================================================
// Ordered set-types:

class GSet
{
    GMap<char> in_set;
    vec<Gate>  gs;

 public:
    // Size operations:
    int      size        (void)      const  { return gs.size(); }
    void     clear       (bool free = false){ 
        if (free)
            in_set.clear(true); 
        else
            for (int i = 0; i < gs.size(); i++)
                in_set[gs[i]] = 0;
        gs.clear(free);
    }
    
    // Vector interface:
    Gate     operator [] (int index) const  { return gs[index]; }


    void     insert      (Gate g) { in_set.growTo(g, 0); if (!in_set[g]) { in_set[g] = 1; gs.push(g); } }
    bool     has         (Gate g) { in_set.growTo(g, 0); return in_set[g]; }
};


class SSet
{
    SMap<char> in_set;
    vec<Sig>   xs;

 public:
    // Size operations:
    int      size        (void)      const  { return xs.size(); }
    void     clear       (bool free = false){ 
        if (free) 
            in_set.clear(true);
        else 
            for (int i = 0; i < xs.size(); i++)
                in_set[xs[i]] = 0;
        xs.clear(free);
    }
    
    // Vector interface:
    Sig      operator [] (int index) const  { return xs[index]; }


    void     insert      (Sig  x) { in_set.growTo(x, 0); if (!in_set[x]) { in_set[x] = 1; xs.push(x); } }
    bool     has         (Sig  x) { in_set.growTo(x, 0); return in_set[x]; }
};

//=================================================================================================
// Fanout type:


class Fanout : private GMap<vec<Gate> >
{

 public:
    // Vector interface:
    vec<Gate>&       operator [] (Gate g)       { return ((vec<vec<Gate> >&)(*this))[index(g)]; }
    const vec<Gate>& operator [] (Gate g) const { return ((const vec<vec<Gate> >&)(*this))[index(g)]; }

    // Note the slightly different semantics to vec's growTo, namely that
    // this guarantees that the element 'g' can be indexed after this operation.
    void     growTo (Gate g)             { ((vec<vec<Gate> >&)(*this)).growTo(index(g) + 1   ); }
    void     clear  (bool free = false)  { ((vec<vec<Gate> >&)(*this)).clear(free); }

    void     attach (Gate from, Gate to) {(*this)[from].push(to);}
};


//=================================================================================================
#endif
