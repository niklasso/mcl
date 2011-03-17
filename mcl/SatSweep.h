/**************************************************************************************[SatSweep.h]
Copyright (c) 2008, Niklas Sorensson

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

#ifndef Minisat_SatSweep_h
#define Minisat_SatSweep_h

#include "minisat/core/Solver.h"
#include "minisat/simp/SimpSolver.h"
#include "mcl/Circ.h"
#include "mcl/CircPrelude.h"
#include "mcl/Clausify.h"
#include "mcl/DagShrink.h"

namespace Minisat {

int  satSweep(Circ& cin, Clausifyer<Solver>& cl, Solver& s, const Eqs& eqs_in, Eqs& eqs_out, int verbosity = 1);
int  satSweep(Circ& cin, Clausifyer<SimpSolver>& cl, SimpSolver& s, const Eqs& eqs_in, Eqs& eqs_out, int verbosity = 1);

void makeUnitClass(const Circ& cin, Eqs& unit);

// NOTE: about to be deleted?
#if 0
//=================================================================================================
// SatSweeper convenience class:
//

template<class Solver>
class SatSweeper
{
 public:
    SatSweeper(const Circ& src, const vec<Sig>& snk);

    void             sweep     ();

    void             copyResult(Circ& out);
    const GMap<Sig>& resultMap ();

    int                verbosity;

 private:

    const Circ&        source;
    const vec<Sig>&    sinks;
    Circ               target;
    Solver             solver;
    Clausifyer<Solver> cl;
    GMap<Sig>          m;
};

//=================================================================================================
// SatSweeper template imlementation:
//


template<class Solver>
inline const GMap<Sig>& SatSweeper<Solver>::resultMap()          { return m; }


template<class Solver>
inline void             SatSweeper<Solver>::copyResult(Circ& out){ GMap<Sig> dummy; out.clear(); copyCirc(target, out, dummy); }


template<class Solver>
SatSweeper<Solver>::SatSweeper(const Circ& src, const vec<Sig>& snk) : verbosity(1), source(src), sinks(snk), cl(source, solver)
{
    // Initialize the target to a clone of the source, and the source to target map to the
    // 'identity map':
    copyCirc(source, target, m);
}


template<class Solver>
void SatSweeper<Solver>::sweep()
{
    // Create the candidate set of equivalences:
    Eqs cand; 
    makeUnitClass(target, cand);

    // printf(" --- SWEEP made unit class.\n");

    // Setup SAT solver:
    solver.rnd_pol = true;

    // Prove which subset of the candidates that holds in the source circuit:
    Eqs proven;
    satSweep(target, cl, solver, cand, proven, verbosity);

    // printf(" --- SWEEP proved equivalences.\n");

    // Copy the whole source circuit while inlining all proven equivalences:
    GMap<Sig> subst;
    makeSubstMap(target, proven, subst);

    // printf(" --- SWEEP created substitution map.\n");

    Circ      sweeped_circ;
    GMap<Sig> sweeped_map;
    copyCircWithSubst(target, sweeped_circ, subst, sweeped_map);

    // printf(" --- SWEEP inlined substitution map.\n");

    // Remap the sinks to point to the sweeped circuit:
    vec<Sig> sweeped_sinks; 
    copy(sinks, sweeped_sinks);
    map(m,           sweeped_sinks); // Remap to point to target circuit.
    map(sweeped_map, sweeped_sinks); // Remap to point to sweeped circuit.

    // printf(" --- SWEEP remapped the set of sinks.\n");

    // Remove redundant logic left in the target circuit:
    DagShrinker dag(sweeped_circ, sweeped_sinks);
    dag.shrink(true);

    // printf(" --- SWEEP removed redundant logic.\n");
    
    // Adjust the target-map and set the target the final result. The result is the composition of:
    // previous 'm', 'sweeped_map', and the result map from removing redundant logic (in that
    // order).
    map(sweeped_map,     m);
    map(dag.resultMap(), m);

    // printf(" --- SWEEP remapped the resulting source to target map.\n");
    
    // Copy the resulting circuit to the target:
    dag.copyResult(target);

    // printf(" --- SWEEP copied the result to target circuit.\n");
}
#endif


};

#endif
