/**************************************************************************************[Matching.h]
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

#ifndef Minisat_Matching_h
#define Minisat_Matching_h

#include "mcl/CircTypes.h"

namespace Minisat {

//=================================================================================================
// Circuit pattern matching functions:

class Circ;

bool matchMuxParts (const Circ& c, Gate g, Gate h, Sig& x, Sig& y, Sig& z);
bool matchMux      (const Circ& c, Gate g, Sig& x, Sig& y, Sig& z);
bool matchXor      (const Circ& c, Gate g, Sig& x, Sig& y);
bool matchXors     (const Circ& c, Gate g, vec<Sig>& tmp_stack, vec<Sig>& xs);
void matchAnds     (const Circ& c, Gate g, GSet& tmp_set, vec<Sig>& tmp_stack, GMap<int>& tmp_fanouts, vec<Sig>& xs, bool match_muxes = false);
void matchTwoLevel (const Circ& c, Gate g, GSet& tmp_set, vec<Sig>& tmp_stack, GMap<int>& tmp_fanouts, vec<vec<Sig> >& xss, bool match_muxes = false);

//=================================================================================================
// CircMatcher -- a helper class for efficient pattern matching of subcircuits:

class CircMatcher
{
    GSet                tmp_set;
    vec<Sig>            tmp_stack;
    GMap<int>           tmp_fanouts;
 public:

    bool matchMuxParts (const Circ& c, Gate g, Gate h, Sig& x, Sig& y, Sig& z);
    bool matchMux      (const Circ& c, Gate g, Sig& x, Sig& y, Sig& z);
    bool matchXor      (const Circ& c, Gate g, Sig& x, Sig& y);
    bool matchXors     (const Circ& c, Gate g, vec<Sig>& xs);
    void matchAnds     (const Circ& c, Gate g, vec<Sig>& xs, bool match_muxes = false);
    void matchTwoLevel (const Circ& c, Gate g, vec<vec<Sig> >& xss, bool match_muxes = false);
};

inline bool CircMatcher::matchMuxParts (const Circ& c, Gate g, Gate h, Sig& x, Sig& y, Sig& z) { return Minisat::matchMuxParts(c, g, h, x, y, z); }
inline bool CircMatcher::matchMux      (const Circ& c, Gate g, Sig& x, Sig& y, Sig& z) { return Minisat::matchMux(c, g, x, y, z); }
inline bool CircMatcher::matchXor      (const Circ& c, Gate g, Sig& x, Sig& y) { return Minisat::matchXor(c, g, x, y); }
inline bool CircMatcher::matchXors     (const Circ& c, Gate g, vec<Sig>& xs) { return Minisat::matchXors(c, g, tmp_stack, xs); }
inline void CircMatcher::matchAnds     (const Circ& c, Gate g, vec<Sig>& xs, bool match_muxes) { Minisat::matchAnds(c, g, tmp_set, tmp_stack, tmp_fanouts, xs, match_muxes); }
inline void CircMatcher::matchTwoLevel (const Circ& c, Gate g, vec<vec<Sig> >& xss, bool match_muxes) { Minisat::matchTwoLevel(c, g, tmp_set, tmp_stack, tmp_fanouts, xss, match_muxes); }

//=================================================================================================
// Debug etc:

};

//=================================================================================================
#endif
