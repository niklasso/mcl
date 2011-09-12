/************************************************************************************[DagShrink.cc]
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

#include "minisat/mtl/Sort.h"
#include "minisat/utils/System.h"
#include "mcl/DagShrink.h"
#include "mcl/CircPrelude.h"

using namespace Minisat;

#define MATCH_MUXANDXOR

//=================================================================================================
// Basic helpers (could be moved to some more generic location):
//

// (stolen from Solver.h)
static inline double drand(double& seed) {
    seed *= 1389796;
    int q = (int)(seed / 2147483647);
    seed -= (double)q * 2147483647;
    return seed / 2147483647; }

// (stolen from Solver.h)
static inline int irand(double& seed, int size) {
    return (int)(drand(seed) * size); }


template<class T>
static void randomShuffle(double& seed, vec<T>& xs)
{
    for (int i = 0; i < xs.size(); i++){
        int pick = i + irand(seed, xs.size() - i);

        assert(pick < xs.size());

        T tmp = xs[i];
        xs[i] = xs[pick];
        xs[pick] = tmp;
    }
}


template<class T>
static void randomShuffle(double& seed, vec<vec<T> >& xs)
{
    for (int i = 0; i < xs.size(); i++){
        int pick = i + irand(seed, xs.size() - i);

        assert(pick < xs.size());

        vec<T> tmp; xs[i].moveTo(tmp);
        xs[pick].moveTo(xs[i]);
        tmp.moveTo(xs[pick]);
    }
}

//=================================================================================================
// Helper functions:
//


static const int cut_off = 100;

static Sig rebuildAnds(Circ& in, CircMatcher& cm, vec<Sig>& xs, double& rnd_seed)
{
    if (xs.size() == 0) return sig_True;

    for (int i = 0; i < xs.size(); i++)
        if (xs[i] == sig_True){
            xs[i] = xs.last();
            xs.pop();
            i--;
        }else if (xs[i] == sig_False)
            return sig_False;

    in.push();


    // Search for already present and-nodes:
    int reused_nodes = 0;
#if 1
    if (xs.size() < cut_off)
        for (int i = 1; i < xs.size(); i++){
            if (xs[i] == sig_Undef) continue;
            assert(gate(xs[i]) != gate_True);

            for (int j = 0; j < i; j++){

                if (xs[j] == sig_Undef) continue;
                assert(xs[j] != sig_True);
                assert(xs[j] != sig_False);

                Sig x = in.tryAnd(xs[i], xs[j]);

                if (x == sig_Undef) continue;

                if (x == sig_False) {
                    // Contradiction found:
                    in.pop();
                    return sig_False;
                }else{
                    if (x != sig_True) reused_nodes++, xs.push(x);
                    xs[i] = xs[j] = sig_Undef;
                    break;
                }
            }
        }
#endif

    int found_muxes = 0;
#if 1
    // Search for mux/xor structures:
    if (xs.size() < cut_off && xs.size() > 2)

        for (int i = 1; i < xs.size(); i++){
            if (xs[i] == sig_Undef || !sign(xs[i]) || type(xs[i]) != gtype_And || in.nFanouts(gate(xs[i])) != 0) continue;
            
            assert(gate(xs[i]) != gate_True);
            //for (int j = 0; j < i; j++){
            for (int j = 0; j < (i > cut_off ? cut_off : i); j++){
                if (xs[j] == sig_Undef || !sign(xs[j]) || type(xs[j]) != gtype_And || in.nFanouts(gate(xs[j])) != 0) continue;
                
                Sig x, y, z;
                if (cm.matchMuxParts(in, gate(xs[i]), gate(xs[j]), x, y, z)){
                    // int gates_before = in.nGates();
                    xs.push(in.mkMux(x, y, z));
                    // assert(gates_before + 1 == in.nGates());
                    xs[i] = xs[j] = sig_Undef;
                    found_muxes++;

                    for (int k = 0; k < xs.size(); k++)
                        if (xs[k] != sig_Undef && xs[k] == ~xs.last()){
                            // Contradiction found:
                            in.pop();
                            return sig_False;
                        }
                    break;
                }
            }
        }
#endif

    // Remove 'sig_Undef's:
    int i, j;
    for (i = j = 0; i < xs.size(); i++)
        if (xs[i] != sig_Undef)
            xs[j++] = xs[i];
    xs.shrink(i - j);


    // Arbitrarily conjoin the rest of the conjunction:
    // TODO: (randomize a balanced tree)
    randomShuffle(rnd_seed, xs);
#if 0
    Sig result = sig_True;
    for (int i = 0; i < xs.size(); i++)
        result = in.mkAnd(result, xs[i]);
#else
    for (int i = 0; i+1 < xs.size(); i += 2)
        xs.push(in.mkAnd(xs[i], xs[i+1]));
    Sig result = xs.size() > 0 ? xs.last() : sig_True;
#endif

    // if (reused_nodes > 0 || found_muxes > 0)
    //     fprintf(stderr, "rebuild-and: reused %d and nodes and found %d muxes\n", reused_nodes, found_muxes);

    return result;
}


static Sig rebuildXors(Circ& in, vec<Sig>& xs, double& rnd_seed)
{
    
    // Search for already present nodes:
    int reused_nodes = 0;
    if (xs.size() > cut_off)
        for (int i = 1; i < xs.size(); i++){
            if (xs[i] == sig_Undef) continue;

            assert(gate(xs[i]) != gate_True);
            for (int j = 0; j < i; j++){
                if (xs[j] == sig_Undef) continue;

                int cost_even = in.costXorEven(xs[i], xs[j]);
                int cost_odd  = in.costXorOdd (xs[i], xs[j]);

#ifndef NDEBUG
                int gates_before = in.nGates();
#endif
                if (cost_even < 3 && cost_even <= cost_odd){
                    xs.push(in.mkXorEven(xs[i], xs[j]));
                    xs[i] = xs[j] = sig_Undef;
                    reused_nodes += 3 - cost_even;
                    assert(gates_before + cost_even == in.nGates());
                    break;
                }else if (cost_odd < 3){
                    xs.push(in.mkXorOdd(xs[i], xs[j]));
                    xs[i] = xs[j] = sig_Undef;
                    reused_nodes += 3 - cost_odd;
                    assert(gates_before + cost_odd == in.nGates());
                    break;
                }
            }
        }

    // Remove 'sig_Undef's:
    int i, j;
    for (i = j = 0; i < xs.size(); i++)
        if (xs[i] != sig_Undef)
            xs[j++] = xs[i];
    xs.shrink(i - j);

    // Arbitrarily chain the rest of the xor:
    // TODO: (randomize a balanced tree?)
    randomShuffle(rnd_seed, xs);
    Sig result = sig_False;
    for (int i = 0; i < xs.size(); i++)
        result = in.mkXorEven(result, xs[i]);

    // if (reused_nodes > 0)
    //     fprintf(stderr, "rebuild-xor: reused %d nodes\n", reused_nodes);

    return result;
}


static Sig rebuildMux(Circ& in, Sig x, Sig y, Sig z)
{
    int a = in.costMuxEven(x, y, z);
    int b = in.costMuxOdd (x, y, z);
    return b < a ? in.mkMuxOdd(x, y, z) : in.mkMuxEven(x, y, z);
}


static inline void printSigs(const vec<Sig>& xs)
{
    fprintf(stderr, "{ ");
    for (int i = 0; i < xs.size(); i++)
        fprintf(stderr, "%s%s%d ", sign(xs[i])?"-":"", type(xs[i]) == gtype_Inp ? "$" : "@", index(gate(xs[i])));
    fprintf(stderr, "}");
}


//=================================================================================================
// Main dagshrink functions:
//

static 
void dagShrink(const Circ& in, Circ& out, vec<Sig>&xs, CircMatcher& cm, GMap<Sig>& map, double& rnd_seed)
{
    randomShuffle(rnd_seed, xs);
    for (int i = 0; i < xs.size(); i++){
        Sig a = dagShrink(in, out, gate(xs[i]), cm, map, rnd_seed);
        assert(a == map[gate(xs[i])]);
        xs[i] = a ^ sign(xs[i]);
    }
}

Sig Minisat::dagShrink(const Circ& in, Circ& out, Gate g, CircMatcher& cm, GMap<Sig>& map, double& rnd_seed)
{
    assert(g != gate_Undef);
    // fprintf(stderr, "Copying gate: %d\n", index(g));

    if (map[g] != sig_Undef) return map[g];
    else if (g == gate_True) return map[g] = sig_True;

    Sig x, y, z, result;

    vec<Sig> xs; 
#ifdef MATCH_MUXANDXOR
    if (cm.matchXors(in, g, xs)){
        ::dagShrink(in, out, xs, cm, map, rnd_seed);
        normalizeXors(xs); // New redundancies may arise after recursive copying/shrinking.
        result = rebuildXors(out, xs, rnd_seed);

    }else if (cm.matchMux(in, g, x, y, z)){
        // fprintf(stderr, "Matched a Mux\n");
    
        x = dagShrink(in, out, gate(x), cm, map, rnd_seed) ^ sign(x);
        y = dagShrink(in, out, gate(y), cm, map, rnd_seed) ^ sign(y);
        z = dagShrink(in, out, gate(z), cm, map, rnd_seed) ^ sign(z);
        result = rebuildMux(out, x, y, z);
    }else 
#endif
    
    if (type(g) == gtype_And){
        cm.matchAnds(in, g, xs);

        ::dagShrink(in, out, xs, cm, map, rnd_seed);
        // normalizeAnds(xs); // New redundancies may arise after recursive copying/shrinking.
        result = rebuildAnds(out, cm, xs, rnd_seed);

    }else {
        //fprintf(stderr, "Matched an input\n");
        assert(type(g) == gtype_Inp);
        result = out.mkInp();
        // fprintf(stderr, "Copying input: %d => %d\n", index(g), index(gate(result)));
    }

    return map[g] = result;
}


// NOTE: about to be deleted ...
#if 0
void Minisat::dagShrink(Circ& c, Box& b, Flops& flp, double& rnd_seed, bool only_copy)
{
    Circ      tmp_circ;
    Box       tmp_box;
    Flops     tmp_flops;
    GMap<Sig> m;
    CircMatcher cm;

    // Copy inputs (including flop gates):
    m.growTo(c.lastGate(), sig_Undef);
    m[gate_True] = sig_True;
    for (int i = 0; i < b.inps.size(); i++) m[b.inps[i]] = tmp_circ.mkInp();
    for (int i = 0; i < flp   .size(); i++) m[flp   [i]] = tmp_circ.mkInp();

    // Build set of all sink-nodes:
    vec<Gate> sinks;
    for (int i = 0; i < b.outs.size(); i++) sinks.push(gate(b.outs[i]));
    for (int i = 0; i < flp   .size(); i++) sinks.push(gate(flp.def(flp[i])));

    // Increase fanouts for all sinks:
    for (int i = 0; i < sinks.size(); i++)
        c.bumpFanout(sinks[i]);

    // Shrink circuit with roots in 'b.outs':
    for (int i = 0; i < sinks.size(); i++)
        if (only_copy) copyGate (c, tmp_circ, sinks[i], m);
        else           dagShrink(c, tmp_circ, sinks[i], cm, m, rnd_seed);

    if (!only_copy){
        dash_stats.current().total_nodes_before = c.nGates();
        dash_stats.current().total_nodes_after  = tmp_circ.nGates();
    }
    
    // Remap inputs, outputs and flops:
    map(m, b);
    map(m, flp);

    // Move circuit back:
    tmp_circ .moveTo(c);
}


void Minisat::dagShrinkIter(Circ& c, Box& b, Flops& flp, int n_iters)
{
    double rnd_seed = 123456789;

    printf("==========================[ DAG Aware Minimization ]===========================\n");
    printf("| TOT. GATES |      MATCHED GATES         |          GAIN        | NEW MUXs   |\n");
    printf("|            | ANDs     XORs     MUXs     | ANDs   XORs   MUXs   |            |\n");
    // printf("| xxxxxxxxxx | xxxxxxxx xxxxxxxx xxxxxxxx | xxxxxx xxxxxx xxxxxx | xxxxxxxxxx |\n");
    printf("===============================================================================\n");

    for (int i = 0; i < n_iters; i++){
        dash_stats.clear();
        dagShrink(c, b, flp, rnd_seed);
        dash_stats.print();
    }
}


void Minisat::dagShrinkIter(Circ& c, Box& b, Flops& flp, double frac)
{
    double rnd_seed = 123456789;

    printf("==========================[ DAG Aware Minimization ]===========================\n");
    printf("| TOT. GATES |      MATCHED GATES         |          GAIN        | NEW MUXs   |\n");
    printf("|            | ANDs     XORs     MUXs     | ANDs   XORs   MUXs   |            |\n");
    // printf("| xxxxxxxxxx | xxxxxxxx xxxxxxxx xxxxxxxx | xxxxxx xxxxxx xxxxxx | xxxxxxxxxx |\n");
    printf("===============================================================================\n");

    int size_before, size_after;
    do {
        dash_stats.clear();
        size_before = c.nGates();
        dagShrink(c, b, flp, rnd_seed);
        size_after = c.nGates();
        dash_stats.print();
    } while (frac < ((double)(size_before - size_after) / size_before));
}


//=================================================================================================
// Utility functions:
//

// TODO: this should be re-evaluated/tested and perhaps reimplemented.
// FIXME: isn't there a normalization step to remove duplicates and check inconsistenies missing at the end?
// Breaking down big output conjunctions, and merging equivalent output nodes:
void Minisat::splitOutputs(Circ& c, Box& b, Flops& flp)
{
    SSet        all_outputs;
    vec<Sig>    xs;
    CircMatcher cm;

    for (;;){
        // bool again = false;
        all_outputs.clear();

        for (int i = 0; i < b.outs.size(); i++){
            Sig x = b.outs[i];
            
            if (type(x) == gtype_And){
                if (!sign(x)){
                    // Handle conjunction:
                    //
                    cm.matchAnds(c, gate(x), xs, true);

                    // printf(" >>> SPLIT OUTPUT [%d] %s%d into %d parts.\n", i, sign(x)?"-":"", index(gate(x)), xs.size());
                    
                    for (int j = 0; j < xs.size(); j++){
                        // if (!sign(xs[j]) && type(xs[j]) == gtype_And)
                        //     again = true;

                        // printf(" >>> hmm: %s%s%d\\%d\n", sign(xs[j])?"-":"", type(xs[j]) == gtype_Inp ? "$":"@", index(gate(xs[j])), c.nFanouts(gate(xs[j])));
                        
                        all_outputs.insert(xs[j]);
                        // splitDisj(c, xs[j], all_outputs);
                    }
                
                    // printf(" >>> GATES = ");
                    // for (int j = 0; j < xs.size(); j++){
                    //     all_outputs.insert(xs[j]);
                    //     printf("%s%s%d ", sign(xs[j])?"-":"", type(xs[j]) == gtype_Inp ? "$":"@", index(gate(xs[j])));
                    // }
                    // printf("\n");
                }else{
                    all_outputs.insert(x);
                    // splitDisj(c, x, all_outputs);
                }
            }else
                all_outputs.insert(x);
        }

        b.outs.clear();
        for (int i = 0; i < all_outputs.size(); i++)
            b.outs.push(all_outputs[i]);

        // FIXME: I don't remember what caused problems with iterating the whole procedure here.
        break;

        // if (!again) 
        //     break;
        // else
        //     printf("AGAIN\n");
    }

    removeDeadLogic(c, b, flp);
}


// Remove logic not reachable from some output or latch:
void Minisat::removeDeadLogic(Circ& c, Box& b, Flops& flp)
{
    double dummy_seed = 123;
    dagShrink(c, b, flp, dummy_seed, true);
}

//=================================================================================================
// New DAG-shrink helper class:
//

DagShrinker::DagShrinker(const Circ& src, const vec<Sig>& snk) : verbosity(1), source(src), sinks(snk), rnd_seed(123456789)
{
    // Initialize the target to a clone of the source, and the source to target map to the
    // 'identity map':
    copyCirc(source, target, m);
}


void DagShrinker::shrink(bool only_copy)
{
    Circ      shrunk_circ;
    GMap<Sig> shrunk_map;

    // Initialize map:
    shrunk_map.growTo(target.lastGate(), sig_Undef);
    shrunk_map[gate_True] = sig_True;

    // Increase fanouts for all sinks. This is an ad-hoc way to make sure that we want to keep
    // these as referable nodes in the resulting circuit:
    for (int i = 0; i < sinks.size(); i++){
        Gate source_g = gate(sinks[i]);
        Gate target_g = gate(m[source_g]);
        assert(source_g != gate_Undef);
        target.bumpFanout(target_g);
    }

    // Shrink circuit reachable from some sink:
    for (int i = 0; i < sinks.size(); i++){
        Gate source_g = gate(sinks[i]);
        Gate target_g = gate(m[source_g]);
        if (only_copy) copyGate (target, shrunk_circ, target_g, shrunk_map);
        else           dagShrink(target, shrunk_circ, target_g, cm, shrunk_map, rnd_seed);
    }

    if (!only_copy){
        dash_stats.current().total_nodes_before = target.nGates();
        dash_stats.current().total_nodes_after  = shrunk_circ.nGates();
    }

    // Adjust the source -> target map:
    map(shrunk_map, m);

    // Move circuit back:
    shrunk_circ.moveTo(target);
}


void  DagShrinker::shrinkIter(int n_iters)
{
    if (verbosity >= 1) printStatsHeader();
    for (int i = 0; i < n_iters; i++){
        shrink();
        if (verbosity >= 1) printStats();
    }
    // printStatsFooter();
}


void  DagShrinker::shrinkIter(double frac)
{
    if (verbosity >= 1) printStatsHeader();
    int size_before, size_after;
    do {
        size_before = target.nGates();
        shrink();
        size_after  = target.nGates();
        if (verbosity >= 1) printStats();
    } while (frac < ((double)(size_before - size_after) / size_before));
    // printStatsFooter();
}


void  DagShrinker::printStatsHeader() const
{
    printf("==========================[ DAG Aware Minimization ]===========================\n");
    printf("| TOT. GATES |      MATCHED GATES         |          GAIN        | NEW MUXs   |\n");
    printf("|            | ANDs     XORs     MUXs     | ANDs   XORs   MUXs   |            |\n");
    printf("===============================================================================\n");
    dash_stats.clear();
}


void  DagShrinker::printStats()       const
{
    dash_stats.print();
    dash_stats.clear();
}


void  DagShrinker::printStatsFooter() const
{
    printf("===============================================================================\n");
}

void  DagShrinker::copyResult(Circ& out){ 
    GMap<Sig> dummy; out.clear(); copyCirc(target, out, dummy); }

#endif
