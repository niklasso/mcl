/****************************************************************************************[Equivs.h]
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

#ifndef Minisat_Equivs_h
#define Minisat_Equivs_h

#include "mcl/CircTypes.h"

namespace Minisat {

//=================================================================================================
// Equivs -- a class for representing equivalences between gates in a circuits:

class Equivs
{
    typedef uint32_t ClassId;
    enum { class_Undef = UINT32_MAX };

    GMap<Sig>       union_find;
    GMap<ClassId>   class_map;
    vec<vec<Sig> >  classes;
    bool            ok;

public:
    Equivs();

    uint32_t        size      ()              const;
    const vec<Sig>& operator[](uint32_t cl)   const;

    Sig             leader    (Sig x)         const;
    bool            merge     (Sig x, Sig y);
    bool            okay      ()              const;
    void            clear     (bool dealloc = false);
    bool            equals    (Sig x, Sig y)  const;

    void            moveTo    (Equivs& to);
    void            copyTo    (Equivs& to) const;
};

inline Equivs::Equivs() : ok(true){}

inline uint32_t        Equivs::size      ()            const { return classes.size(); }
inline const vec<Sig>& Equivs::operator[](uint32_t cl) const { assert(cl < (uint32_t)classes.size()); return classes[cl]; }
inline Sig             Equivs::leader    (Sig x)       const {
    while (union_find.has(gate(x)) && union_find[gate(x)] != sig_Undef)
        x = union_find[gate(x)] ^ sign(x);
    return x;
}
inline bool            Equivs::okay      ()            const { return ok; }
inline bool            Equivs::equals    (Sig x, Sig y)const { return leader(x) == leader(y); }


void equivsUnion       (const Equivs& e, const Equivs& f, Equivs& g);
void equivsIntersection(const Equivs& e, const Equivs& f, Equivs& g);

//=================================================================================================

};

#endif
