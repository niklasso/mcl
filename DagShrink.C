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

#include "DagShrink.h"
#include "Sort.h"


//=================================================================================================
// Statistics:
//

static int nof_muxes_found      = 0;
static int nof_shared_and_nodes = 0;
static int nof_shared_xor_nodes = 0;
static int nof_shared_mux_nodes = 0;

static int total_nodes_before = 0;
static int total_nodes_after = 0;
static int nof_mux_nodes = 0;
static int nof_xor_nodes = 0;
static int nof_and_nodes = 0;
static int total_and_size = 0;
static int total_xor_size = 0;


static void clearDagShrinkStats()
{
    nof_muxes_found = 0;
    nof_shared_and_nodes = 0;
    nof_shared_xor_nodes = 0;
    nof_shared_mux_nodes = 0;

    total_nodes_before = 0;
    total_nodes_after = 0;

    nof_mux_nodes = 0;
    nof_xor_nodes = 0;
    nof_and_nodes = 0;
    total_and_size = 0;
    total_xor_size = 0;
}


static void printDagShrinkStats()
{
    fprintf(stderr, "==========================[ DAG Aware Minimization ]===========================\n");
    fprintf(stderr, "|  Input gates      = %8d                                                |\n", total_nodes_before);
    fprintf(stderr, "|  Input and nodes  = %8d  ( %6.2f avg. size)                           |\n", nof_and_nodes, (float)total_and_size / nof_and_nodes);
    fprintf(stderr, "|  Input xor nodes  = %8d  ( %6.2f avg. size)                           |\n", nof_xor_nodes, (float)total_xor_size / nof_xor_nodes);
    fprintf(stderr, "|  Input mux nodes  = %8d                                                |\n", nof_mux_nodes);
    fprintf(stderr, "|  Output gates     = %8d                                                |\n", total_nodes_after);
    fprintf(stderr, "|  Muxes introduced = %8d                                                |\n", nof_muxes_found);
    fprintf(stderr, "|  Shared and nodes = %8d                                                |\n", nof_shared_and_nodes);
    fprintf(stderr, "|  Shared xor nodes = %8d                                                |\n", nof_shared_xor_nodes);
    fprintf(stderr, "|  Shared mux nodes = %8d                                                |\n", nof_shared_mux_nodes);

}

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

//=================================================================================================
// Helper functions:
//


static Sig rebuildAnds(Circ& in, vec<Sig>& xs, double& rnd_seed)
{
    const int cut_off = 1000;

    if (xs.size() > cut_off)
        fprintf(stderr, "large conjunction: %d\n", xs.size());


    // Search for already present and-nodes:
    int reused_nodes = 0;
    for (int i = 1; i < xs.size(); i++){
        if (xs[i] == sig_Undef) continue;

        assert(gate(xs[i]) != gate_True);
        // for (int j = 0; j < i; j++)
        for (int j = 0; j < (i > cut_off ? cut_off : i); j++)
            if (xs[j] != sig_Undef && in.costAnd(xs[i], xs[j]) == 0){
                xs.push(in.mkAnd(xs[i], xs[j]));
                xs[i] = xs[j] = sig_Undef;
                reused_nodes++;
                break;
            }
    }

    // Search for mux/xor structures:
    int found_muxes = 0;
    if (xs.size() > 2) // This is a hack to improve statistics somewhat.

        for (int i = 1; i < xs.size(); i++){
            if (xs[i] == sig_Undef || !sign(xs[i]) || type(xs[i]) != gtype_And) continue;
            
            assert(gate(xs[i]) != gate_True);
            //for (int j = 0; j < i; j++){
            for (int j = 0; j < (i > cut_off ? cut_off : i); j++){
                if (xs[j] == sig_Undef || !sign(xs[j]) || type(xs[j]) != gtype_And) continue;
                
                // FIXME: should we really pair together parts that have
                // external fanouts? It is confusing, at least for the statistics. 
                Sig x, y, z;
                if (in.matchMuxParts(gate(xs[i]), gate(xs[j]), x, y, z)){
                    /* 
                       if (gate(x) == mkGate(79, gtype_Inp)){
                       fprintf(stderr, " >>> FOUND MUX: %s%d, %s%s%d, %s%s%d\n", 
                       type(x) == gtype_Inp ? "$" : "@", index(gate(x)), 
                       sign(y)?"-":"", type(y) == gtype_Inp ? "$" : "@", index(gate(y)), 
                       sign(z)?"-":"", type(z) == gtype_Inp ? "$" : "@", index(gate(z))
                       );
                       
                       fprintf(stderr, " >>> CONTAINING CONJUNCTION: ");
                       for (int i = 0; i < xs.size(); i++)
                       fprintf(stderr, "%s%s%d ", sign(xs[i])?"-":"", type(xs[i]) == gtype_Inp ? "$" : "@", index(gate(xs[i])));
                       fprintf(stderr, "\n");
                       
                       
                       }
                    */
                    xs.push(in.mkMux(x, y, z));
                    xs[i] = xs[j] = sig_Undef;
                    found_muxes++;
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


    // Arbitrarily conjoin the rest of the conjunction:
    // TODO: (randomize a balanced tree)
    randomShuffle(rnd_seed, xs);
    Sig result = sig_True;
    for (int i = 0; i < xs.size(); i++)
        result = in.mkAnd(result, xs[i]);

    // if (reused_nodes > 0 || found_muxes > 0)
    //     fprintf(stderr, "rebuild-and: reused %d and nodes and found %d muxes\n", reused_nodes, found_muxes);

    nof_shared_and_nodes += reused_nodes;
    nof_muxes_found += found_muxes;

    return result;
}


static Sig rebuildXors(Circ& in, vec<Sig>& xs, double& rnd_seed)
{
    // Search for already present nodes:
    int reused_nodes = 0;
    for (int i = 1; i < xs.size(); i++){
        if (xs[i] == sig_Undef) continue;

        assert(gate(xs[i]) != gate_True);
        for (int j = 0; j < i; j++){
            if (xs[j] == sig_Undef) continue;

            int cost_even = in.costXorEven(xs[i], xs[j]);
            int cost_odd  = in.costXorOdd (xs[i], xs[j]);

            if (cost_even < 3 && cost_even <= cost_odd){
                xs.push(in.mkXorEven(xs[i], xs[j]));
                xs[i] = xs[j] = sig_Undef;
                reused_nodes += 3 - cost_even;
                break;
            }else if (cost_odd < 3){
                xs.push(in.mkXorOdd(xs[i], xs[j]));
                xs[i] = xs[j] = sig_Undef;
                reused_nodes += 3 - cost_odd;
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
    nof_shared_xor_nodes += reused_nodes;

    return result;
}


static Sig rebuildMux(Circ& in, Sig x, Sig y, Sig z)
{
    int a = in.costMuxEven(x, y, z);
    int b = in.costMuxOdd (x, y, z);
    
    // if (a < 3 || b < 3)
    //     fprintf(stderr, "rebuilding mux: a = %d, b = %d\n", a, b);
    int best = b < a ? b : a;

    if (best < 3) nof_shared_mux_nodes += (3 - best);

    /*
    bool slask = false;
    if (best < 3 && gate(x) == mkGate(602, gtype_Inp) && type(y) == gtype_Inp && type(z) == gtype_Inp){
        fprintf(stderr, " >>> IMPROVED MUX: %s%d, %s%s%d, %s%s%d (%d)\n", 
                type(x) == gtype_Inp ? "$" : "@", index(gate(x)), 
                sign(y)?"-":"", type(y) == gtype_Inp ? "$" : "@", index(gate(y)), 
                sign(z)?"-":"", type(z) == gtype_Inp ? "$" : "@", index(gate(z)),
                best);
        
        // fprintf(stderr, " >>> CONTAINING CONJUNCTION: ");
        // for (int i = 0; i < xs.size(); i++)
        //     fprintf(stderr, "%s%s%d ", sign(xs[i])?"-":"", type(xs[i]) == gtype_Inp ? "$" : "@", index(gate(xs[i])));
        // fprintf(stderr, "\n");
        slask = true;
    }

    Sig result = b < a ? in.mkMuxOdd(x, y, z) : in.mkMuxEven(x, y, z);

    Sig left = in.lchild(result);
    Sig right = in.rchild(result);

    if (slask){
        fprintf(stderr, " >>> EVEN COST: %d\n", a);
        fprintf(stderr, " >>> ODD COST: %d\n", b);
        fprintf(stderr, " >>> REWRITE TARGET: %s%s%d = (%s%s%d(%d), %s%s%d(%d))\n",
                sign(result)?"-":"", type(result) == gtype_Inp ? "$":"@", index(gate(result)),
                sign(left)?"-":"", type(left) == gtype_Inp ? "$":"@", index(gate(left)), in.nFanouts(gate(left)),
                sign(right)?"-":"", type(right) == gtype_Inp ? "$":"@", index(gate(right)), in.nFanouts(gate(right)));
    }
    return result;
    */
    
    return b < a ? in.mkMuxOdd(x, y, z) : in.mkMuxEven(x, y, z);
}


Sig  dagShrink(Circ& in, Circ& out, Gate g,       GMap<Sig>& map, double& rnd_seed);
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


Sig dagShrink(Circ& in, Circ& out, Gate g, GMap<Sig>& map, double& rnd_seed)
{
    // fprintf(stderr, "Copying gate: %d\n", index(g));

    if (map[g] != sig_Undef) return map[g];
    else if (g == gate_True) return map[g] = sig_True;

    Sig x, y, z, result;
    vec<Sig> xs; 

    if (in.matchXors(g, xs)){
        dagShrink(in, out, xs, map, rnd_seed);
        normalizeXors(xs); // New redundancies may arise after recursive copying/shrinking.

        nof_xor_nodes++; total_xor_size += xs.size();
        result = rebuildXors(out, xs, rnd_seed);
    
    }else if (in.matchMux(g, x, y, z)){
        // fprintf(stderr, "Matched a Mux\n");
    
        x = dagShrink(in, out, gate(x), map, rnd_seed) ^ sign(x);
        y = dagShrink(in, out, gate(y), map, rnd_seed) ^ sign(y);
        z = dagShrink(in, out, gate(z), map, rnd_seed) ^ sign(z);
        result = rebuildMux(out, x, y, z);

        nof_mux_nodes++;
    
    }else if (type(g) == gtype_And){
        in.matchAnds(g, xs);

        normalizeAnds(xs); // Remove redundancies so that dead parts of the circuit are not copied.
        dagShrink(in, out, xs, map, rnd_seed);
        normalizeAnds(xs); // New redundancies may arise after recursive copying/shrinking.

        nof_and_nodes++; total_and_size += xs.size();
        result = rebuildAnds(out, xs, rnd_seed);

    }else {
        //fprintf(stderr, "Matched an input\n");
        assert(type(g) == gtype_Inp);
        result = out.mkInp();
        // fprintf(stderr, "Copying input: %d => %d\n", index(g), index(gate(result)));
    }

    return map[g] = result;
}


void findDefs(AigerCirc& c)
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


void dagShrink(AigerCirc& c, double& rnd_seed, bool only_copy)
{
    Circ      tmp; 
    GMap<Sig> map;

    // findDefs(c);

    // Copy inputs & latches:
    c.circ.adjustMapSize(map, sig_Undef);
    for (int i = 0; i < c.inputs.size(); i++)  map[c.inputs[i]]  = tmp.mkInp();
    for (int i = 0; i < c.latches.size(); i++) map[c.latches[i]] = tmp.mkInp();

    if (!only_copy) clearDagShrinkStats();
    // Shrink circuit with roots in 'outputs' and 'latch_defs':
    for (int i = 0; i < c.outputs.size(); i++)
        if (only_copy) copyGate (c.circ, tmp, gate(c.outputs[i]), map);
        else           dagShrink(c.circ, tmp, gate(c.outputs[i]), map, rnd_seed);

    for (int i = 0; i < c.latches.size(); i++)
        if (only_copy) copyGate (c.circ, tmp, gate(c.latch_defs[c.latches[i]]), map);
        else           dagShrink(c.circ, tmp, gate(c.latch_defs[c.latches[i]]), map, rnd_seed);
    if (!only_copy){
        total_nodes_before = c.circ.nGates();
        total_nodes_after  = tmp.nGates();
        printDagShrinkStats(); }

    // Move circuit back:
    tmp.moveTo(c.circ);

    // Remap inputs:
    for (int i = 0; i < c.inputs.size(); i++){
        assert(c.inputs[i] == gate(map[c.inputs[i]]));
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


//=================================================================================================
// Utility functions:
//

// Breaking down big output conjunctions:
void splitOutputs(AigerCirc& c)
{
    SSet     all_outputs;
    vec<Sig> xs;

    for (int i = 0; i < c.outputs.size(); i++){
        Sig x = c.outputs[i];

        if (!sign(x) && type(x) == gtype_And){
            c.circ.matchAnds(gate(x), xs, true);
            // fprintf(stderr, " >>> SPLIT OUTPUT [%d] %s%d into %d parts.\n", i, sign(x)?"-":"", index(gate(x)), xs.size());

            for (int j = 0; j < xs.size(); j++)
                all_outputs.insert(xs[j]);

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

    removeDeadLogic(c);
}


// Remove logic not reachable from some output or latch:
void removeDeadLogic(AigerCirc& c)
{
    double dummy_seed = 123;
    dagShrink(c, dummy_seed, true);
}
