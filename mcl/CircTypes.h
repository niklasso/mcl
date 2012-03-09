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

#ifndef Minisat_CircTypes_h
#define Minisat_CircTypes_h

#include "minisat/mtl/IntTypes.h"
#include "minisat/mtl/Vec.h"

namespace Minisat {

//=================================================================================================
// Helper types (analogues of Var/Lit types from MiniSat):

typedef enum { gtype_Inp = 0, gtype_And = 1, gtype_Const = 2 } GateType;

//-------------------------------------------------------------------------------------------------
// Gate-type: (analogue of MiniSat's Var)


// --- bit 0: type { 0 = inp, 1 = and }, 1 : unused, 2-31 data ---
struct Gate {
    unsigned int x;
    bool operator == (Gate p) const { return x == p.x; }
    bool operator != (Gate p) const { return x != p.x; }
    bool operator <  (Gate p) const { return x <  p.x; }
    bool operator <= (Gate p) const { return x <= p.x; }
    bool operator >  (Gate p) const { return x >  p.x; }
    bool operator >= (Gate p) const { return x >= p.x; }
};

const Gate gate_Undef = { ((1U << 30)-1) << 2 };
const Gate gate_True  = { 0 };

// Use this as a constructor:
inline Gate         mkGate  (unsigned int id, GateType t){
    Gate g; g.x = (id << 2) + (unsigned int)(t == gtype_And); return g; }
inline GateType     type    (Gate g){ return g == gate_True ? gtype_Const : GateType(g.x & 1); }

// Note! Use GMap instead of this:
inline unsigned int index (Gate g) { return g.x >> 2; }

//-------------------------------------------------------------------------------------------------
// Signal-type: (analogue of MiniSat's Lit)

// --- bit 0: type { 0 = inp, 1 = and }, 1 : sign, 2-31 data ---
struct Sig {
    unsigned int x;
    bool operator == (Sig p) const { return x == p.x; }
    bool operator != (Sig p) const { return x != p.x; }
    bool operator <  (Sig p) const { return x <  p.x; } // '<' makes p, ~p adjacent in the ordering.
    bool operator <= (Sig p) const { return x <= p.x; }
    bool operator >  (Sig p) const { return x >  p.x; }
    bool operator >= (Sig p) const { return x >= p.x; }
};

//const Sig lit_Undef = mkSig(var_Undef, false);  // }- Useful special constants.
//const Sig lit_Error = mkSig(var_Undef, true );  // }

const Sig sig_Undef = { ((1U << 30)-1) << 2 };  // }- Useful special constants.
const Sig sig_Error = { (((1U << 30)-1) << 2) + 2};  // }
const Sig sig_True  = { 0 };
const Sig sig_False = { 2 };

// Use this as a constructor:
inline  Sig mkSig(Gate g, bool sign = false){ 
    Sig p; p.x = g.x + (unsigned)sign + (unsigned)sign; return p; }

// TODO: make sure 'sig_Undef == ~sig_Undef' and 'sig_Error == ~sig_Error'?
inline  Sig      operator ~(Sig p)                       { Sig q; q.x = p.x ^ 2; return q; }
inline  Sig      operator ^(Sig p, bool b)               { Sig q; q.x = p.x ^ (((unsigned int)b)<<1); return q; }
inline  bool     sign      (Sig p)                       { return bool(p.x & 2); }
inline  Gate     gate      (Sig p)                       { return mkGate(p.x >> 2, GateType(p.x & 1)); }
inline  GateType type      (Sig p)                       { return type(gate(p)); }

// Mapping Signals to and from compact integers suitable for array indexing:
inline  Sig          toSig     (unsigned  i)         { Sig p; p.x = i; return p; } 

// Note! Use SMap instead of this:
inline unsigned int index (Sig s) { return s.x >> 1; }


//=================================================================================================
// Map-types:

template<class T>
class GMap : private vec<T>
{
 public:
    // Create new GMap with zero capacity:
    GMap(){}                                      

    // Create new GMap with capacity to store gate 'g', initializing table-entries with 'e':
    GMap(Gate g, const T& e){ growTo(g, e); }

    // FIXME: I think this works for empty vectors (gmaps), but I need to check.
    typedef T* iterator;
    iterator begin  (){ return &vec<T>::operator[](0); }
    iterator end    (){ return &vec<T>::operator[](vec<T>::size()); }

    // Vector interface:
    const T& operator [] (Gate g)  const { assert(g != gate_Undef); assert(index(g) < (unsigned)vec<T>::size()); return vec<T>::operator[](index(g)); }
    T&       operator [] (Gate g)        { assert(g != gate_Undef); assert(index(g) < (unsigned)vec<T>::size()); return vec<T>::operator[](index(g)); }

    // Note the slightly different semantics to vec's growTo, namely that
    // this guarantees that the element 'g' can be indexed after this operation.
    void     growTo (Gate g)             { vec<T>::growTo(index(g) + 1   ); }
    void     growTo (Gate g, const T& e) { vec<T>::growTo(index(g) + 1, e); }
    void     shrink (int size)           { vec<T>::shrink(size); }

    bool     has    (Gate g)      const  { return index(g) < (unsigned)vec<T>::size(); }
    void     clear  (bool free = false)  { vec<T>::clear(free); }
    int      size   () const             { return vec<T>::size(); }

    void     moveTo (GMap<T>& to)        { vec<T>::moveTo((vec<T>&)to); }
    void     copyTo (GMap<T>& to) const  { vec<T>::copyTo((vec<T>&)to); }
};


template<class T>
class SMap : private vec<T>
{
 public:
    // Create new SMap with zero capacity:
    SMap(){}                                      

    // Create new SMap with capacity to store signal 'x', initializing table-entries with 'e':
    SMap(Sig x, const T& e){ growTo(x, e); }

    // FIXME: I think this works for empty vectors (smaps), but I need to check.
    typedef T* iterator;
    iterator begin  (){ return &vec<T>::operator[](0); }
    iterator end    (){ return &vec<T>::operator[](vec<T>::size()); }

    // Vector interface:
    const T& operator [] (Sig x) const   { assert(x != sig_Undef); assert(index(x) < (unsigned)vec<T>::size()); return vec<T>::operator[](index(x)); }
    T&       operator [] (Sig x)         { assert(x != sig_Undef); assert(index(x) < (unsigned)vec<T>::size()); return vec<T>::operator[](index(x)); }

    // Note the slightly different semantics to vec's capacity, namely that
    // this guarantees that the element 'g' can be indexed after this operation.
    void     growTo (Sig x)              { vec<T>::growTo(index(x) + 1   ); }
    void     growTo (Sig x, const T& e)  { vec<T>::growTo(index(x) + 1, e); }
    void     shrink (int size)           { vec<T>::shrink(size); }

    bool     has    (Sig x)       const  { return index(x) < (unsigned)vec<T>::size(); }
    void     clear  (bool free = false)  { vec<T>::clear(free); }
    int      size   () const             { return vec<T>::size(); }

    void     moveTo (SMap<T>& to)        { vec<T>::moveTo((vec<T>&)to); }
    void     copyTo (SMap<T>& to) const  { vec<T>::copyTo((vec<T>&)to); }
};


#if 0
//-------------------------------------------------------------------------------------------------
// Helper-class for SGMap:
// FIXME: this is quite general, move somewhere else?

template<class T>
struct SignedRef {
    T* ref;

    SignedRef(T* r, bool s) : ref((T*) ((uintptr_t)r | (int)s)) {}

    // Assignment operator:
    void operator=(const T& value){ 
        T* r = (T*)((uintptr_t)ref & ~1);
        *r = value ^ ((uintptr_t)ref & 1); }

    // Automatic type-conversion operator:
    operator T (void){ 
        T* r = (T*)((uintptr_t)ref & ~1);
        return *r ^ ((uintptr_t)ref & 1); }

    // Helps resolving overloading:
    bool operator==(const T& other){ return other == *this; }
    bool operator!=(const T& other){ return other != *this; }
};

template<class T>
class SGMap : private vec<T>
{
 public:
    // Vector interface:
    SignedRef<T> operator [](Sig x){ 
        T*           ref  = &((vec<T>&)(*this))[index(gate(x))];
        SignedRef<T> sref(ref, sign(x));
        return sref; }


    // Note the slightly different semantics to vec's capacity, namely that
    // this guarantees that the element 'g' can be indexed after this operation.
    void     growTo (Sig x)             { ((vec<T>&)(*this)).growTo(index(gate(x)) + 1   ); }
    void     growTo (Sig x, const T& e) { ((vec<T>&)(*this)).growTo(index(gate(x)) + 1, e); }

    bool     has    (Sig x)       const { return index(gate(x)) < (uint32_t)size(); }
    void     clear  (bool free = false) { ((vec<T>&)(*this)).clear(free); }
    int      size   () const            { return ((vec<T>&)(*this)).size(); }

    void     moveTo (SGMap<T>& to)      { ((vec<T>&)(*this)).moveTo((vec<T>&)to); }
    void     copyTo (SGMap<T>& to) const{ ((vec<T>&)(*this)).copyTo((vec<T>&)to); }
};
#endif

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
    
    // Allow inspecting the internal vector:
    const vec<Gate>&
             toVec       ()          const  { return gs; }
        
    // Vector interface:
    Gate     operator [] (int index) const  { return gs[index]; }


    void     insert      (Gate g) { in_set.growTo(g, 0); if (!in_set[g]) { in_set[g] = 1; gs.push(g); } }
    bool     has         (Gate g) const { return in_set.has(g) && in_set[g]; }
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

    // Allow inspecting the internal vector:
    const vec<Sig>&
             toVec       ()          const  { return xs; }

    void     insert      (Sig  x) { in_set.growTo(x, 0); if (!in_set[x]) { in_set[x] = 1; xs.push(x); } }
    bool     has         (Sig  x) const { return in_set.has(x) && in_set[x]; }
};


//=================================================================================================

};

#endif
