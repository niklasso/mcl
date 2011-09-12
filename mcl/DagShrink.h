/*************************************************************************************[DagShrink.h]
Copyright (c) 2008-2011, Niklas Sorensson

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

#ifndef Minisat_DagShrink_h
#define Minisat_DagShrink_h

#include "mcl/Aiger.h"
#include "mcl/Circ.h"
#include "mcl/Matching.h"

namespace Minisat {

Sig  dagShrink       (const Circ& in, Circ& out, Gate g, CircMatcher& cm, GMap<Sig>& m, double& rnd_seed);

// NOTE: about to be deleted...
#if 0
void dagShrink       (Circ& c, Box& b, Flops& flp, double& rnd_seed, bool only_copy = false);
void dagShrinkIter   (Circ& c, Box& b, Flops& flp, int    n_iters = 5);
void dagShrinkIter   (Circ& c, Box& b, Flops& flp, double frac);
void splitOutputs    (Circ& c, Box& b, Flops& flp);
void removeDeadLogic (Circ& c, Box& b, Flops& flp);


class DagShrinker
{
 public:
    DagShrinker(const Circ& src, const vec<Sig>& snk);

    void  shrink(bool only_copy = false);
    void  shrinkIter(int n_iters);
    void  shrinkIter(double frac);

    void  printStatsHeader() const;
    void  printStats()       const;
    void  printStatsFooter() const;

    Sig   lookup(Gate g); // Source Gate -> Target Gate (not sure if this is needed, or safe)
    Sig   lookup(Sig  x); // Source Sig  -> Target Sig

    const Circ&      result();
    const GMap<Sig>& resultMap();
    void             copyResult(Circ& out);

    int              verbosity;

 private:

    const Circ&      source;
    const vec<Sig>&  sinks;
    Circ             target;
    CircMatcher      cm;
    GMap<Sig>        m;
    double           rnd_seed;

};


inline const Circ&      DagShrinker::result()             { return target; }
inline const GMap<Sig>& DagShrinker::resultMap()          { return m; }
#endif

};

#endif
