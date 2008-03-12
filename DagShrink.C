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
#if 0
    fprintf(stderr, "==========================[ DAG Aware Minimization ]===========================\n");
    fprintf(stderr, "|  Input gates      = %8d                                                |\n", total_nodes_before);
    fprintf(stderr, "|  Input and nodes  = %8d  ( %6.2g avg. size)                           |\n", nof_and_nodes, (float)total_and_size / nof_and_nodes);
    fprintf(stderr, "|  Input xor nodes  = %8d  ( %6.2g avg. size)                           |\n", nof_xor_nodes, (float)total_xor_size / nof_xor_nodes);
    fprintf(stderr, "|  Input mux nodes  = %8d                                                |\n", nof_mux_nodes);
    fprintf(stderr, "|  Output gates     = %8d                                                |\n", total_nodes_after);
    fprintf(stderr, "|  Muxes introduced = %8d                                                |\n", nof_muxes_found);
    fprintf(stderr, "|  Shared and nodes = %8d                                                |\n", nof_shared_and_nodes);
    fprintf(stderr, "|  Shared xor nodes = %8d                                                |\n", nof_shared_xor_nodes);
    fprintf(stderr, "|  Shared mux nodes = %8d                                                |\n", nof_shared_mux_nodes);
    fprintf(stderr, "|  cpu-time         = %8.2f                                                |\n", cpuTime() - time_before);
#else
    // fprintf(stderr, "| xxxxxxxxxx | xxxxxxxx xxxxxxxx xxxxxxxx | xxxxxx xxxxxx xxxxxx | xxxxxxxxxx |\n");
    fprintf(stderr, "| %10d | %8d %8d %8d | %6d %6d %6d | %10d |\n", total_nodes_after, nof_and_nodes, nof_xor_nodes, nof_mux_nodes, nof_shared_and_nodes, nof_shared_xor_nodes, nof_shared_mux_nodes, nof_muxes_found);
#endif
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
    if (xs.size() < cut_off)
        for (int i = 1; i < xs.size(); i++){
            if (xs[i] == sig_Undef) continue;
            
            assert(gate(xs[i]) != gate_True);
            for (int j = 0; j < i; j++){

                assert(xs[j] != sig_True);
                assert(xs[j] != sig_False);

                if (xs[j] != sig_Undef && in.costAnd(xs[i], xs[j]) == 0){
                    xs.push(in.mkAnd(xs[i], xs[j]));
                    assert(xs.last() != sig_True);
                    assert(xs.last() != sig_False);
                    xs[i] = xs[j] = sig_Undef;
                    reused_nodes++;
                    
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
                    int gates_before = in.nGates();
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

                int gates_before = in.nGates();

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


static void printSigs(const vec<Sig>& xs)
{
    fprintf(stderr, "{ ");
    for (int i = 0; i < xs.size(); i++)
        fprintf(stderr, "%s%s%d ", sign(xs[i])?"-":"", type(xs[i]) == gtype_Inp ? "$" : "@", index(gate(xs[i])));
    fprintf(stderr, "}");
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

            return odd;
        }else{
            in.pop();
            dash_stats.pop();
            return rebuildTwoLevelEven(in, xss, rnd_seed);
        }
    }
}


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


Sig Minisat::dagShrink(Circ& in, Circ& out, Gate g, GMap<Sig>& map, double& rnd_seed)
{
    // fprintf(stderr, "Copying gate: %d\n", index(g));

    if (map[g] != sig_Undef) return map[g];
    else if (g == gate_True) return map[g] = sig_True;

    Sig x, y, z, result;

#if 1
    vec<Sig> xs; 
    if (in.matchXors(g, xs)){
        ::dagShrink(in, out, xs, map, rnd_seed);
        // normalizeXors(xs); // New redundancies may arise after recursive copying/shrinking.

        dash_stats.current().nof_xor_nodes++; dash_stats.current().total_xor_size += xs.size();
        result = rebuildXors(out, xs, rnd_seed);

    }else if (in.matchMux(g, x, y, z)){
        // fprintf(stderr, "Matched a Mux\n");
    
        x = dagShrink(in, out, gate(x), map, rnd_seed) ^ sign(x);
        y = dagShrink(in, out, gate(y), map, rnd_seed) ^ sign(y);
        z = dagShrink(in, out, gate(z), map, rnd_seed) ^ sign(z);
        result = rebuildMux(out, x, y, z);

        dash_stats.current().nof_mux_nodes++;
#endif
    
#if 1
    }else if (type(g) == gtype_And){
        in.matchAnds(g, xs);

        ::dagShrink(in, out, xs, map, rnd_seed);
        // normalizeAnds(xs); // New redundancies may arise after recursive copying/shrinking.

        dash_stats.current().nof_and_nodes++; dash_stats.current().total_and_size += xs.size();
        result = rebuildAnds(out, xs, rnd_seed);
#else
    }else if (type(g) == gtype_And){
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


void Minisat::dagShrink(AigerCirc& c, double& rnd_seed, bool only_copy)
{
    Circ      tmp; 
    GMap<Sig> map;

    // findDefs(c);

    // Copy inputs & latches:
    c.circ.adjustMapSize(map, sig_Undef);
    for (int i = 0; i < c.inputs.size(); i++)  map[c.inputs[i]]  = tmp.mkInp();
    for (int i = 0; i < c.latches.size(); i++) map[c.latches[i]] = tmp.mkInp();

    // Shrink circuit with roots in 'outputs' and 'latch_defs':
    for (int i = 0; i < c.outputs.size(); i++)
        if (only_copy) copyGate (c.circ, tmp, gate(c.outputs[i]), map);
        else           dagShrink(c.circ, tmp, gate(c.outputs[i]), map, rnd_seed);

    for (int i = 0; i < c.latches.size(); i++)
        if (only_copy) copyGate (c.circ, tmp, gate(c.latch_defs[c.latches[i]]), map);
        else           dagShrink(c.circ, tmp, gate(c.latch_defs[c.latches[i]]), map, rnd_seed);
    if (!only_copy){
        dash_stats.current().total_nodes_before = c.circ.nGates();
        dash_stats.current().total_nodes_after  = tmp.nGates();
    }

    // Move circuit back:
    tmp.moveTo(c.circ);

    // Remap inputs:
    for (int i = 0; i < c.inputs.size(); i++){
        // Should I make the follwing assertion hold?
        // assert(c.inputs[i] == gate(map[c.inputs[i]]));
        c.inputs[i] = gate(map[c.inputs[i]]);
    }

    // Remap outputs:
    for (int i = 0; i < c.outputs.size(); i++){
        assert(map[gate(c.outputs[i])] != sig_Undef);
        c.outputs[i] = map[gate(c.outputs[i])] ^ sign(c.outputs[i]);
    }

    // Remap latches & latch_defs:
    c.latch_defs.clear();
    c.circ.adjustMapSize(c.latch_defs, sig_Undef);
    for (int i = 0; i < c.latches.size(); i++){
        assert(map[c.latches[i]] != sig_Undef);
        assert(!sign(map[c.latches[i]]));
        assert(c.latch_defs[c.latches[i]] != sig_Undef);
        assert(map[gate(c.latch_defs[c.latches[i]])] != sig_Undef);

        Sig x = map[c.latches[i]];
        Sig d = map[gate(c.latch_defs[c.latches[i]])] ^ sign(c.latch_defs[c.latches[i]]);

        c.latches[i]          = gate(x);
        c.latch_defs[gate(x)] = d;
    }
}


void Minisat::dagShrink(AigerCirc& c, int n_iters)
{
    double rnd_seed = 123456789;

    fprintf(stderr, "==========================[ DAG Aware Minimization ]===========================\n");
    fprintf(stderr, "| TOT. GATES |      MATCHED GATES         |          GAIN        | NEW MUXs   |\n");
    fprintf(stderr, "|            | ANDs     XORs     MUXs     | ANDs   XORs   MUXs   |            |\n");
    // fprintf(stderr, "| xxxxxxxxxx | xxxxxxxx xxxxxxxx xxxxxxxx | xxxxxx xxxxxx xxxxxx | xxxxxxxxxx |\n");
    fprintf(stderr, "===============================================================================\n");

    for (int i = 0; i < n_iters; i++){
        dash_stats.clear();
        dagShrink(c, rnd_seed);
        dash_stats.print();
    }
}


//=================================================================================================
// Utility functions:
//

// Breaking down big output conjunctions, and merging equivalent output nodes:
void Minisat::splitOutputs(AigerCirc& c)
{
    SSet     all_outputs;
    vec<Sig> xs;

    for (;;){
        bool again = false;
        all_outputs.clear();

        for (int i = 0; i < c.outputs.size(); i++){
            Sig x = c.outputs[i];
            
            if (!sign(x) && type(x) == gtype_And){
                c.circ.matchAnds(gate(x), xs, true);
                // fprintf(stderr, " >>> SPLIT OUTPUT [%d] %s%d into %d parts.\n", i, sign(x)?"-":"", index(gate(x)), xs.size());
                
                for (int j = 0; j < xs.size(); j++){
                    if (!sign(xs[j]) && type(xs[j]) == gtype_And)
                        again = true;

                    // fprintf(stderr, " >>> hmm: %s%s%d\\%d\n", sign(xs[j])?"-":"", type(xs[j]) == gtype_Inp ? "$":"@", index(gate(xs[j])), c.circ.nFanouts(gate(xs[j])));
                    
                    all_outputs.insert(xs[j]);
                }
                
                // fprintf(stderr, " >>> GATES = ");
                // for (int j = 0; j < xs.size(); j++){
                //     all_outputs.insert(xs[j]);
                //     fprintf(stderr, "%s%s%d ", sign(xs[j])?"-":"", type(xs[j]) == gtype_Inp ? "$":"@", index(gate(xs[j])));
                // }
                // fprintf(stderr, "\n");
            }
            else
                all_outputs.insert(x);
        }

        c.outputs.clear();
        for (int i = 0; i < all_outputs.size(); i++)
            c.outputs.push(all_outputs[i]);

        break;

        // if (!again) 
        //     break;
        // else
        //     fprintf(stderr, "AGAIN\n");
    }

    removeDeadLogic(c);
}


// Remove logic not reachable from some output or latch:
void Minisat::removeDeadLogic(AigerCirc& c)
{
    double dummy_seed = 123;
    dagShrink(c, dummy_seed, true);
}


//=================================================================================================
// Armin Biere and Robert Brummaier's simple two-level AIG simplification:
//


static int biere_num_contr = 0;
static int biere_num_subsump = 0;
static int biere_num_idem = 0;
static int biere_num_res = 0;
static int biere_num_subst = 0;

static Sig biereShrink(Circ& in, Circ& out, Gate g, GMap<Sig>& map);

static Sig biereShrink(Circ& in, Circ& out, Sig  x, GMap<Sig>& map){
    return biereShrink(in, out, gate(x), map) ^ sign(x); }

static Sig biereShrink(Circ& in, Circ& out, Gate g, GMap<Sig>& map)
{
    if (map[g] != sig_Undef) return map[g];
    else if (g == gate_True) return map[g] = sig_True;
    else if (type(g) == gtype_Inp)
        return map[g] = out.mkInp();
    else{
        // 'g' is an and-gate:

        Sig l  = biereShrink(in, out, in.lchild(g), map);
        Sig r  = biereShrink(in, out, in.rchild(g), map);
        
        Sig ll = type(l) == gtype_Inp ? sig_Undef : out.lchild(l);
        Sig lr = type(l) == gtype_Inp ? sig_Undef : out.rchild(l);

        Sig rl = type(r) == gtype_Inp ? sig_Undef : out.lchild(r);
        Sig rr = type(r) == gtype_Inp ? sig_Undef : out.rchild(r);

        // Level two rules:
        //
        if      (!sign(l) && type(l) == gtype_And &&             (ll == ~r || lr == ~r))                             return biere_num_contr++, map[g] = sig_False; // Contradiction 1.1
        else if (!sign(r) && type(r) == gtype_And &&             (rl == ~l || rr == ~l))                             return biere_num_contr++, map[g] = sig_False; // Contradiction 1.2
        else if (!sign(l) && type(l) == gtype_And && !sign(r) && type(r) == gtype_And && (ll == ~rl || ll == ~rr || lr == ~rl || lr == ~rr)) return biere_num_contr++, map[g] = sig_False; // Contradiction 2

#if 1
        else if ( sign(l) && type(l) == gtype_And && (ll == ~r || lr == ~r))                             return biere_num_subsump++, map[g] = r;         // Subsumption 1.1
        else if ( sign(r) && type(r) == gtype_And && (rl == ~l || rr == ~l))                             return biere_num_subsump++, map[g] = l;         // Subsumption 1.2

        else if ( sign(l) && type(l) == gtype_And && !sign(r) && type(r) == gtype_And && (ll == ~rl || ll == ~rr || lr == ~rl || lr == ~rr)) return biere_num_subsump++, map[g] = r;         // Subsumption 2.1
        else if (!sign(l) && type(l) == gtype_And &&  sign(r) && type(r) == gtype_And && (ll == ~rl || ll == ~rr || lr == ~rl || lr == ~rr)) return biere_num_subsump++, map[g] = l;         // Subsumption 2.2
#endif

#if 1
        else if (!sign(l) && type(l) == gtype_And && (ll ==  r || lr ==  r))                             return biere_num_idem++, map[g] = l;         // Idempotency 1.1
        else if (!sign(r) && type(r) == gtype_And && (rl ==  l || rr ==  l))                             return biere_num_idem++, map[g] = r;         // Idempotency 1.2
#endif

#if 1
        else if ( sign(l) && type(l) == gtype_And &&  sign(r) && type(r) == gtype_And && ( (ll == rl && lr == ~rr) || (ll == rr && lr == ~rl) )) return biere_num_res++, map[g] = ~ll;    // Resolution 1.1
        else if ( sign(l) && type(l) == gtype_And &&  sign(r) && type(r) == gtype_And && ( (lr == rl && ll == ~rr) || (lr == rr && ll == ~rl) )) return biere_num_res++, map[g] = ~lr;    // Resolution 1.1
#endif

#if 1
        else if ( sign(l) && type(l) == gtype_And && (ll == r)) return biere_num_subst++, map[g] = out.mkAnd(~lr, r);               // Substitution 1.1
        else if ( sign(l) && type(l) == gtype_And && (lr == r)) return biere_num_subst++, map[g] = out.mkAnd(~ll, r);               // Substitution 1.2
        else if ( sign(r) && type(r) == gtype_And && (rl == l)) return biere_num_subst++, map[g] = out.mkAnd(~rr, l);               // Substitution 1.3
        else if ( sign(r) && type(r) == gtype_And && (rr == l)) return biere_num_subst++, map[g] = out.mkAnd(~rl, l);               // Substitution 1.4


        else if ( sign(l) && type(l) == gtype_And && !sign(r) && type(r) == gtype_And && (ll == rl || ll == rr)) return biere_num_subst++, map[g] = out.mkAnd(~lr, r);  // Substitution 2.1
        else if ( sign(l) && type(l) == gtype_And && !sign(r) && type(r) == gtype_And && (lr == rl || lr == rr)) return biere_num_subst++, map[g] = out.mkAnd(~ll, r);  // Substitution 2.2
        else if (!sign(l) && type(l) == gtype_And &&  sign(r) && type(r) == gtype_And && (rl == ll || rl == lr)) return biere_num_subst++, map[g] = out.mkAnd(~rr, l);  // Substitution 2.3
        else if (!sign(l) && type(l) == gtype_And &&  sign(r) && type(r) == gtype_And && (rr == ll || rr == lr)) return biere_num_subst++, map[g] = out.mkAnd(~rl, l);  // Substitution 2.4
#endif
        else
            return map[g] = out.mkAnd(l, r);
    }
        
}

void Minisat::biereShrink(AigerCirc& c)
{
    Circ      tmp; 
    GMap<Sig> map;

    // findDefs(c);

    // Copy inputs & latches:
    c.circ.adjustMapSize(map, sig_Undef);
    for (int i = 0; i < c.inputs.size(); i++)  map[c.inputs[i]]  = tmp.mkInp();
    for (int i = 0; i < c.latches.size(); i++) map[c.latches[i]] = tmp.mkInp();

    // Shrink circuit with roots in 'outputs' and 'latch_defs':
    for (int i = 0; i < c.outputs.size(); i++)
        ::biereShrink(c.circ, tmp, gate(c.outputs[i]), map);

    for (int i = 0; i < c.latches.size(); i++)
        ::biereShrink(c.circ, tmp, gate(c.latch_defs[c.latches[i]]), map);

    // Move circuit back:
    tmp.moveTo(c.circ);

    // Remap inputs:
    for (int i = 0; i < c.inputs.size(); i++){
        // Should I make the follwing assertion hold?
        // assert(c.inputs[i] == gate(map[c.inputs[i]]));
        c.inputs[i] = gate(map[c.inputs[i]]);
    }

    // Remap outputs:
    for (int i = 0; i < c.outputs.size(); i++){
        assert(map[gate(c.outputs[i])] != sig_Undef);
        c.outputs[i] = map[gate(c.outputs[i])] ^ sign(c.outputs[i]);
    }

    // Remap latches & latch_defs:
    c.latch_defs.clear();
    c.circ.adjustMapSize(c.latch_defs, sig_Undef);
    for (int i = 0; i < c.latches.size(); i++){
        assert(map[c.latches[i]] != sig_Undef);
        assert(!sign(map[c.latches[i]]));
        assert(c.latch_defs[c.latches[i]] != sig_Undef);
        assert(map[gate(c.latch_defs[c.latches[i]])] != sig_Undef);

        Sig x = map[c.latches[i]];
        Sig d = map[gate(c.latch_defs[c.latches[i]])] ^ sign(c.latch_defs[c.latches[i]]);

        c.latches[i]          = gate(x);
        c.latch_defs[gate(x)] = d;
    }

    removeDeadLogic(c);

    fprintf(stderr, "|  # contradictions = %8d                                                |\n", biere_num_contr);
    fprintf(stderr, "|  # subsumptions   = %8d                                                |\n", biere_num_subsump);
    fprintf(stderr, "|  # idempotencies  = %8d                                                |\n", biere_num_idem);
    fprintf(stderr, "|  # resolutions    = %8d                                                |\n", biere_num_res);
    fprintf(stderr, "|  # substitutions  = %8d                                                |\n", biere_num_subst);
    fprintf(stderr, "===============================================================================\n");
}
