/*************************************************************************************[DagShrink.C]
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

#include "mtl/Sort.h"
#include "circ/DagShrink.h"
#include "utils/System.h"

using namespace Minisat;

#define MATCH_MUXANDXOR
#define MATCH_TWOLEVEL

//=================================================================================================
// Statistics:
//

struct DagShrinkStatsFrame
{
    int    nof_muxes_found;
    int    nof_shared_and_nodes;
    int    nof_shared_xor_nodes;
    int    nof_shared_mux_nodes;
    int    total_nodes_before;
    int    total_nodes_after;
    int    nof_mux_nodes;
    int    nof_xor_nodes;
    int    nof_and_nodes;
    int    total_and_size;
    int    total_xor_size;
    int    node_cnt;
    double time_before;

    void   clear();
    void   print();
};


class DagShrinkStats
{
    vec<DagShrinkStatsFrame> stats_stack;
public:
    DagShrinkStats() { stats_stack.push(); stats_stack.last().clear(); }

    void clear();
    void print();
    void push();
    void pop();
    void commit();

    DagShrinkStatsFrame& current();
};


void DagShrinkStatsFrame::clear()
{
    nof_muxes_found      = 0;
    nof_shared_and_nodes = 0;
    nof_shared_xor_nodes = 0;
    nof_shared_mux_nodes = 0;
    total_nodes_before   = 0;
    total_nodes_after    = 0;
    nof_mux_nodes        = 0;
    nof_xor_nodes        = 0;
    nof_and_nodes        = 0;
    total_and_size       = 0;
    total_xor_size       = 0;
    node_cnt             = 0;
    time_before          = cpuTime();
}


void DagShrinkStatsFrame::print()
{
    // fprintf(stderr, "| xxxxxxxxxx | xxxxxxxx xxxxxxxx xxxxxxxx | xxxxxx xxxxxx xxxxxx | xxxxxxxxxx |\n");
    printf("| %10d | %8d %8d %8d | %6d %6d %6d | %10d |\n", total_nodes_after, nof_and_nodes, nof_xor_nodes, nof_mux_nodes, nof_shared_and_nodes, nof_shared_xor_nodes, nof_shared_mux_nodes, nof_muxes_found);
}


DagShrinkStatsFrame& 
     DagShrinkStats::current() { return stats_stack.last(); }
void DagShrinkStats::clear  () { stats_stack.last().clear(); }
void DagShrinkStats::print  () { stats_stack.last().print(); }
void DagShrinkStats::push   () { stats_stack.push(stats_stack.last()); }
void DagShrinkStats::pop    () { stats_stack.pop(); }
void DagShrinkStats::commit () {
    assert(stats_stack.size() > 1);
    DagShrinkStatsFrame sf = stats_stack.last();
    stats_stack.pop(); 
    stats_stack.last() = sf;
}


static DagShrinkStats dash_stats;


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

static Sig rebuildAnds(Circ& in, vec<Sig>& xs, double& rnd_seed)
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
                if (in.matchMuxParts(gate(xs[i]), gate(xs[j]), x, y, z)){
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

    if (result != sig_True){
        in.commit();
        dash_stats.current().nof_shared_and_nodes += reused_nodes;
        dash_stats.current().nof_muxes_found += found_muxes;
    }else
        in.pop();

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
    dash_stats.current().nof_shared_xor_nodes += reused_nodes;

    return result;
}


static Sig rebuildMux(Circ& in, Sig x, Sig y, Sig z)
{
    int a = in.costMuxEven(x, y, z);
    int b = in.costMuxOdd (x, y, z);
    
    int best = b < a ? b : a;

    if (best < 3) dash_stats.current().nof_shared_mux_nodes += (3 - best);

    return b < a ? in.mkMuxOdd(x, y, z) : in.mkMuxEven(x, y, z);
}


static inline void printSigs(const vec<Sig>& xs)
{
    fprintf(stderr, "{ ");
    for (int i = 0; i < xs.size(); i++)
        fprintf(stderr, "%s%s%d ", sign(xs[i])?"-":"", type(xs[i]) == gtype_Inp ? "$" : "@", index(gate(xs[i])));
    fprintf(stderr, "}");
}




#ifdef MATCH_TWOLEVEL
static Sig rebuildTwoLevelEven(Circ& in, const vec<vec<Sig> >& xss, double& rnd_seed)
{
    vec<Sig> top;
    vec<Sig> inv_clause;

    for (int i = 0; i < xss.size(); i++){
        xss[i].copyTo(inv_clause);
        for (int j = 0; j < inv_clause.size(); j++)
            inv_clause[j] = ~inv_clause[j];
        top.push(~rebuildAnds(in, inv_clause, rnd_seed));
    }

    return rebuildAnds(in, top, rnd_seed);
}


static bool convertDNFtoCNF(const vec<vec<Sig> >& dnf, vec<vec<Sig> >& cnf)
{
    static const int cut_off = 100;
    cnf.push();

    vec<vec<Sig> > tmp;
    // fprintf(stderr, " >>> Converting CNF to DNF:\n");
    // for (int i = 0; i < dnf.size(); i++){
    //     fprintf(stderr, " --- ");
    //     printSigs(dnf[i]);
    //     fprintf(stderr, "\n");
    // }


    for (int i = 0; i < dnf.size(); i++){
        // Expand current cnf with the product in dnf[i]:

        tmp.clear();
        for (int j = 0; j < cnf.size(); j++)
            for (int k = 0; k < dnf[i].size(); k++){
                Sig x = dnf[i][k];

                if (find(cnf[j], x)){
                    tmp.push();
                    cnf[j].copyTo(tmp.last());
                }else if (!find(cnf[j], ~x)){
                    tmp.push();
                    cnf[j].copyTo(tmp.last());
                    tmp.last().push(x);
                }
                
            }
        tmp.moveTo(cnf);

        if (cnf.size() > cut_off)
            return false;
    }

    normalizeTwoLevel(cnf);
    return true;
}


static Sig rebuildTwoLevelOdd(Circ& in, const vec<vec<Sig> >& xss, double& rnd_seed)
{
    vec<vec<Sig> > tmp_dnf;

    for (int i = 0; i < xss.size(); i++){
        tmp_dnf.push();
        for (int j = 0; j < xss[i].size(); j++)
            tmp_dnf.last().push(~xss[i][j]);
    }

    vec<vec<Sig> > new_cnf;

    if (convertDNFtoCNF(tmp_dnf, new_cnf))
        return ~rebuildTwoLevelEven(in, new_cnf, rnd_seed);
    else
        return sig_Undef;
}


static Sig rebuildTwoLevel(Circ& in, vec<vec<Sig> >& xss, double& rnd_seed)
{
    double tmp_seed;

    if (xss.size() == 1 && xss[0].size() == 0){
        // fprintf(stderr, "matched a constant false two-level.\n");
        return sig_False;
    }
    else if (xss.size() == 0){
        // fprintf(stderr, "matched a constant true two-level.\n");
        return  sig_True;
    }

    in.push();

    dash_stats.push();

    tmp_seed = rnd_seed;
    Sig even = rebuildTwoLevelEven(in, xss, tmp_seed);
    uint32_t gates_even = in.nGates();

    if (xss.size() > 8){
        //if (true){
        in.commit();
        dash_stats.commit();

        return even;
    }else{
        in.pop();
        in.push();
        dash_stats.pop();
        dash_stats.push();

        tmp_seed = rnd_seed;
        Sig odd = rebuildTwoLevelOdd(in, xss, tmp_seed);
        uint32_t gates_odd = (odd != sig_Undef) ? in.nGates() : UINT32_MAX;
        
        if (gates_odd < gates_even){
            in.commit();
            dash_stats.commit();
            // printf("<<< GOT HERE...\n");
            return odd;
        }else{
            in.pop();
            dash_stats.pop();
            return rebuildTwoLevelEven(in, xss, rnd_seed);
        }
    }
}
#endif

//=================================================================================================
// Main dagshrink functions:
//

static 
void dagShrink(Circ& in, Circ& out, vec<Sig>& xs, GMap<Sig>& map, double& rnd_seed)
{
    randomShuffle(rnd_seed, xs);
    for (int i = 0; i < xs.size(); i++){
        Sig a = dagShrink(in, out, gate(xs[i]), map, rnd_seed);
        assert(a == map[gate(xs[i])]);
        xs[i] = a ^ sign(xs[i]);
    }
}
#ifdef MATCH_TWOLEVEL
static 
void dagShrink(Circ& in, Circ& out, vec<vec<Sig> >& xss, GMap<Sig>& map, double& rnd_seed)
{
    randomShuffle(rnd_seed, xss);
    for (int i = 0; i < xss.size(); i++){
        randomShuffle(rnd_seed, xss[i]);
        for (int j = 0; j < xss[i].size(); j++){
            Sig a = dagShrink(in, out, gate(xss[i][j]), map, rnd_seed);
            assert(a == map[gate(xss[i][j])]);
            xss[i][j] = a ^ sign(xss[i][j]);
        }
    }
    normalizeTwoLevel(xss);
}
#endif

Sig Minisat::dagShrink(Circ& in, Circ& out, Gate g, GMap<Sig>& map, double& rnd_seed)
{
    // fprintf(stderr, "Copying gate: %d\n", index(g));

    if (map[g] != sig_Undef) return map[g];
    else if (g == gate_True) return map[g] = sig_True;

    Sig x, y, z, result;

    vec<Sig> xs; 
#ifdef MATCH_MUXANDXOR
    if (in.matchXors(g, xs)){
        ::dagShrink(in, out, xs, map, rnd_seed);
        normalizeXors(xs); // New redundancies may arise after recursive copying/shrinking.

        dash_stats.current().nof_xor_nodes++; dash_stats.current().total_xor_size += xs.size();
        result = rebuildXors(out, xs, rnd_seed);

    }else if (in.matchMux(g, x, y, z)){
        // fprintf(stderr, "Matched a Mux\n");
    
        x = dagShrink(in, out, gate(x), map, rnd_seed) ^ sign(x);
        y = dagShrink(in, out, gate(y), map, rnd_seed) ^ sign(y);
        z = dagShrink(in, out, gate(z), map, rnd_seed) ^ sign(z);
        result = rebuildMux(out, x, y, z);

        dash_stats.current().nof_mux_nodes++;
    }else 
#endif
    
#ifndef MATCH_TWOLEVEL
    if (type(g) == gtype_And){
        in.matchAnds(g, xs);

        ::dagShrink(in, out, xs, map, rnd_seed);
        // normalizeAnds(xs); // New redundancies may arise after recursive copying/shrinking.

        dash_stats.current().nof_and_nodes++; dash_stats.current().total_and_size += xs.size();
        result = rebuildAnds(out, xs, rnd_seed);
#else
    if (type(g) == gtype_And){
        //if (type(g) == gtype_And){
        vec<vec<Sig> > xss;
        in.matchTwoLevel(g, xss, false);

        ::dagShrink(in, out, xss, map, rnd_seed);

        for (int i = 0; i < xss.size(); i++)
            if (xss[i].size() > 1){
                dash_stats.current().nof_and_nodes++;
                dash_stats.current().total_and_size += xss[i].size();
            }
        dash_stats.current().nof_and_nodes++;
        dash_stats.current().total_and_size += xss.size();

        // if (dash_stats.current().node_cnt++ % 100 == 0)
        //     fprintf(stderr, "PROGRESS: %d\r", dash_stats.current().node_cnt);
                
        result = rebuildTwoLevel(out, xss, rnd_seed);
#endif

    }else {
        //fprintf(stderr, "Matched an input\n");
        assert(type(g) == gtype_Inp);
        result = out.mkInp();
        // fprintf(stderr, "Copying input: %d => %d\n", index(g), index(gate(result)));
    }

    return map[g] = result;
}


#if 0
static void findDefs(AigerCirc& c)
{
    vec<Sig> xs;
    for (int i = 0; i < c.outputs.size(); i++)
        if (!sign(c.outputs[i])){
            xs.clear();
            c.circ.matchAnds(gate(c.outputs[i]), xs);
            fprintf(stderr, " >>> OUTPUT %d is a conjunction of size %d\n", i, xs.size());

            Sig x, y;
            int n_defs = 0;
            int n_xors = 0;
            for (int j = 0; j < xs.size(); j++)
                //if (c.circ.matchXor(gate(xs[i]), x, y) && (type(x) == gtype_Inp || type(y) == gtype_Inp))
                if (c.circ.matchXor(gate(xs[i]), x, y) && (n_xors++, (type(x) == gtype_Inp || type(y) == gtype_Inp)))
                    n_defs++;

            fprintf(stderr, " >>> OUTPUT %d contains %d definition-type XORS (%d total)\n", i, n_defs, n_xors);
        }
        
}
#endif


void Minisat::dagShrink(Circ& c, Box& b, Flops& flp, double& rnd_seed, bool only_copy)
{
    Circ      tmp_circ;
    Box       tmp_box;
    Flops     tmp_flops;
    GMap<Sig> map;

    // Copy inputs (including flop gates):
    c.adjustMapSize(map, sig_Undef);
    for (int i = 0; i < b.inps.size(); i++) map[b.inps[i]] = tmp_circ.mkInp();
    for (int i = 0; i < flp   .size(); i++) map[flp   [i]] = tmp_circ.mkInp();

    // Build set of all sink-nodes:
    vec<Gate> sinks;
    for (int i = 0; i < b.outs.size(); i++) sinks.push(gate(b.outs[i]));
    for (int i = 0; i < flp   .size(); i++) sinks.push(gate(flp.def(flp[i])));

    // Increase fanouts for all sinks:
    for (int i = 0; i < sinks.size(); i++)
        c.bumpFanout(sinks[i]);

    // Shrink circuit with roots in 'b.outs':
    for (int i = 0; i < sinks.size(); i++)
        if (only_copy) copyGate (c, tmp_circ, sinks[i], map);
        else           dagShrink(c, tmp_circ, sinks[i], map, rnd_seed);

    if (!only_copy){
        dash_stats.current().total_nodes_before = c.nGates();
        dash_stats.current().total_nodes_after  = tmp_circ.nGates();
    }

    // Remap inputs, outputs and flops:
    b  .remap(map, tmp_box);
    flp.remap(tmp_circ, map, tmp_flops);

    // Move circuit back:
    tmp_circ .moveTo(c);
    tmp_box  .moveTo(b);
    tmp_flops.moveTo(flp);
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

// NOTE: this appears to be buggy ...
static inline void splitDisj(Circ& c, Sig x, SSet& all_outputs)
{
    // if (sign(x) && type(x) == gtype_And){
    if (false){
        // printf(" >>> got here ... \n");

        // Handle disjunction:
        //
        Sig l = c.lchild(x);
        Sig r = c.rchild(x);

        if (!sign(r) || type(r) != gtype_And){ Sig tmp = l; l = r; r = tmp; }
    
        if (!sign(r) || type(r) != gtype_And)
            all_outputs.insert(x);
        else{
            vec<Sig> xs;
            if (sign(l) && type(l) == gtype_And){
                int size_l = (c.matchAnds(gate(l), xs, true), xs.size());
                int size_r = (c.matchAnds(gate(r), xs, true), xs.size());
                
                if (size_l > size_r){ Sig tmp = l; l = r; r = tmp; }
            }
        
            // x is signed, r is signed (i.e x == ~l or ~r):
            assert(sign(r));
            assert(type(r) == gtype_And);
            c.matchAnds(gate(r), xs, true);

            // printf(" >>> DISTRIBUTED DISJ. OVER CONJ. OF SIZE %d\n", xs.size());
            for (int j = 0; j < xs.size(); j++)
                all_outputs.insert(c.mkOr(~l, xs[j]));
        }
    }else
        all_outputs.insert(x);
}

// Breaking down big output conjunctions, and merging equivalent output nodes:
void Minisat::splitOutputs(Circ& c, Box& b, Flops& flp)
{
    SSet     all_outputs;
    vec<Sig> xs;

    for (;;){
        bool again = false;
        all_outputs.clear();

        for (int i = 0; i < b.outs.size(); i++){
            Sig x = b.outs[i];
            
            if (type(x) == gtype_And){
                if (!sign(x)){
                    // Handle conjunction:
                    //
                    c.matchAnds(gate(x), xs, true);

                    // printf(" >>> SPLIT OUTPUT [%d] %s%d into %d parts.\n", i, sign(x)?"-":"", index(gate(x)), xs.size());
                    
                    for (int j = 0; j < xs.size(); j++){
                        if (!sign(xs[j]) && type(xs[j]) == gtype_And)
                            again = true;

                        // printf(" >>> hmm: %s%s%d\\%d\n", sign(xs[j])?"-":"", type(xs[j]) == gtype_Inp ? "$":"@", index(gate(xs[j])), c.nFanouts(gate(xs[j])));
                        
                        // all_outputs.insert(xs[j]);
                        splitDisj(c, xs[j], all_outputs);
                    }
                
                    // printf(" >>> GATES = ");
                    // for (int j = 0; j < xs.size(); j++){
                    //     all_outputs.insert(xs[j]);
                    //     printf("%s%s%d ", sign(xs[j])?"-":"", type(xs[j]) == gtype_Inp ? "$":"@", index(gate(xs[j])));
                    // }
                    // printf("\n");
                }else{
                    splitDisj(c, x, all_outputs);
                }
            }else
                all_outputs.insert(x);
        }

        b.outs.clear();
        for (int i = 0; i < all_outputs.size(); i++)
            b.outs.push(all_outputs[i]);
        // break;

        if (!again) 
            break;
        else
            printf("AGAIN\n");
    }

    removeDeadLogic(c, b, flp);
}


// Remove logic not reachable from some output or latch:
void Minisat::removeDeadLogic(Circ& c, Box& b, Flops& flp)
{
    double dummy_seed = 123;
    dagShrink(c, b, flp, dummy_seed, true);
}
