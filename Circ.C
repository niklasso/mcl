/******************************************************************************************[Circ.C]
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

#include "Circ.h"

//=================================================================================================
// Circ members:


Circ::Circ() 
    : n_inps     (0) 
    , n_ands     (0)
    , strash     (NULL)
    , strash_cap (0) 
    , tmp_gate   (gate_True)
{ 
    gates.growTo(tmp_gate); 
    restrashAll();
    gates[tmp_gate].strash_next = gate_Undef;
}


void Circ::clear()
{
    gates.clear();
    n_fanouts.clear();
    n_inps = 0;
    n_ands = 0;
    if (strash) free(strash);
    strash = NULL;
    strash_cap = 0;
    
    gates.growTo(tmp_gate); 
    restrashAll();
    gates[tmp_gate].strash_next = gate_Undef;
}


void Circ::moveTo(Circ& to)
{
    gates.moveTo(to.gates);
    n_fanouts.moveTo(to.n_fanouts);
    to.n_inps = n_inps;
    to.n_ands = n_ands;
    if (to.strash) free(to.strash);
    to.strash = strash;
    to.strash_cap = strash_cap;

    n_inps = 0;
    n_ands = 0;
    strash = NULL;
    strash_cap = 0;

    gates.growTo(tmp_gate); 
    restrashAll();
    gates[tmp_gate].strash_next = gate_Undef;
}


void Circ::restrashAll()
{
    static const unsigned int nprimes   = 47;
    static const unsigned int primes [] = { 31, 47, 71, 107, 163, 251, 379, 569, 853, 1279, 1931, 2897, 4349, 6529, 9803, 14713, 22073, 33113, 49669, 74507, 111767, 167663, 251501, 377257, 565889, 848839, 1273267, 1909907, 2864867, 4297301, 6445951, 9668933, 14503417, 21755137, 32632727, 48949091, 73423639, 110135461, 165203191, 247804789, 371707213, 557560837, 836341273, 1254511933, 1881767929, 2822651917U, 4233977921U };

    // Find new size:
    unsigned int oldsize = strash_cap;
    strash_cap  = primes[0];
    for (unsigned int i = 1; strash_cap <= oldsize && i < nprimes; i++)
        strash_cap = primes[i];

    // printf("New strash size: %d\n", strash_cap);

    // Allocate and initialize memory for new table:
    strash = (Gate*)realloc(strash, sizeof(Gate) * strash_cap);
    for (unsigned int i = 0; i < strash_cap; i++)
        strash[i] = gate_Undef;

    // Rehash active and-nodes into new table:
    for (Gate g = firstGate(); g != gate_Undef; g = nextGate(g))
        if (type(g) == gtype_And) 
            strashInsert(g);
}


//=================================================================================================
// Circ utility functions:


// Given certain values for inputs, calculate the values of all gates in the cone of influence
// of a signal:
//
bool evaluate(const Circ& c, Sig x, GMap<lbool>& values)
{
    Gate g = gate(x);
    values.growTo(g, l_Undef);
    if (values[g] == l_Undef){
        assert(type(g) == gtype_And);
        values[g] = lbool(evaluate(c, c.lchild(g), values) && evaluate(c, c.rchild(g), values));
        //printf("%d = %s%d & %s%d ==> %d\n", index(g), sign(c.lchild(g)) ? "-":"", index(gate(c.lchild(g))), sign(c.rchild(g)) ? "-":"", index(gate(c.rchild(g))),
        //       toInt(values[g]));

    }
    assert(values[g] != l_Undef);
    return (values[g] ^ sign(x)) == l_True;
}



//=================================================================================================
// Generate bottomUp topological orders:
//
void bottomUpOrder(const Circ& c, Sig  x, GSet& gset) { bottomUpOrder(c, gate(x), gset); }
void bottomUpOrder(const Circ& c, Gate g, GSet& gset)
{
    if (gset.has(g) || g == gate_True) return;

    if (type(g) == gtype_And){
        bottomUpOrder(c, gate(c.lchild(g)), gset);
        bottomUpOrder(c, gate(c.rchild(g)), gset);
    }
    gset.insert(g);
}


void bottomUpOrder(const Circ& c, const vec<Gate>& gs, GSet& gset)
{
    for (int i = 0; i < gs.size(); i++)
        bottomUpOrder(c, gs[i], gset);
}


void bottomUpOrder(const Circ& c, const vec<Sig>& xs, GSet& gset)
{
    for (int i = 0; i < xs.size(); i++)
        bottomUpOrder(c, xs[i], gset);
}


void bottomUpOrder(const Circ& c, const vec<Gate>& latches, const GMap<Sig>& latch_defs, GSet& gset)
{
    bool repeat;
    do {
        repeat = false;
        for (int i = 0; i < latches.size(); i++){
            Gate g = latches[i];
            Gate d = gate(latch_defs[g]);
            
            if (gset.has(g) && !gset.has(d)){
                bottomUpOrder(c, d, gset);
                repeat = true;
            }
        }
    } while (repeat);
}

//=================================================================================================
// Big-and/Big-xor normalization functions:
//


void normalizeXors(vec<Sig>& xs)
{
    // Calculate the parity and make inputs unsigned:
    //
    bool pol = false;
    for (int i = 0; i < xs.size(); i++){
        pol   = pol ^ sign(xs[i]);
        xs[i] = mkSig(gate(xs[i]));
    }

    // Sort and remove duplicates properly:
    //
    int  i, j, k;
    sort(xs);
    for (i = j = 0; i < xs.size(); i = k){
        // Check how many times xs[i] is repeated:
        for (k = i; k < xs.size() && xs[k] == xs[i]; k++)
            ;

        // fprintf(stderr, "i = %d, j = %d, k = %d, k - i = %d\n", i, j, k, k - i);

        // Only if xs[i] occurs an odd number of times it should be kept:
        if ((k - i) % 2 == 1)
            xs[j++] = xs[i];
    }
    xs.shrink(i - j);

    // Remove possible remaining constant (can only be one, and it must occur first):
    if (xs.size() > 0 && xs[0] == sig_True){
        for (i = 1; i < xs.size(); i++)
            xs[i-1] = xs[i];
        xs.shrink(1);
        pol = !pol;
    }

    if (xs.size() > 0)
        // Attach the sign to the last element:
        xs.last() = xs.last() ^ pol;
    else if (pol)
        // If the xor-chain is empty but the output is expected to be negated, add a constant true:
        xs.push(sig_True);
}


void normalizeAnds(vec<Sig>& xs)
{
    int  i, j;
    sort(xs);

    // Remove duplicates and detect inconsistencies:
    for (i = j = 1; i < xs.size(); i++)
        if (xs[i] == ~xs[j-1]){
            xs.clear();
            xs.push(sig_False);
            return;
        }else if (xs[i] != xs[j-1])
            xs[j++] = xs[i];

    xs.shrink(i - j);

    // Remove constant true:
    if (xs.size() > 0 && xs[0] == sig_True){
        for (i = 1; i < xs.size(); i++)
            xs[i-1] = xs[i];
        xs.shrink(1);
    }

    // Detect constant false:
    if (xs.size() > 0 && xs[0] == sig_False){
        xs.clear();
        xs.push(sig_False);
    }
}


//=================================================================================================
// Circuit pattern matching functions:
//

bool matchMuxParts(const Circ& c, Gate g, Gate h, Sig& x, Sig& y, Sig& z)
{
    Sig ll = c.lchild(g);
    Sig lr = c.rchild(g);
    Sig rl = c.lchild(h);
    Sig rr = c.rchild(h);

    assert(ll < lr);
    assert(rl < rr);

    // Find condition signal:
    bool is_mux = true;
    if      (ll == ~rl){ x = ll; y = ~lr; z = ~rr; } 
    else if (lr == ~rl){ x = lr; y = ~ll; z = ~rr; } 
    else if (ll == ~rr){ x = ll; y = ~lr; z = ~rl; }
    else if (lr == ~rr){ x = lr; y = ~ll; z = ~rl; } 
    else is_mux = false;

    // Normalize:
    if (is_mux && sign(x)){ Sig tmp = y; y = z; z = tmp; x = ~x; }

    /*
    if (is_mux && gate(x) == mkGate(602, gtype_Inp) && type(y) == gtype_Inp && type(z) == gtype_Inp){
        fprintf(stderr, " >>> NODE (%d, %d) MATCHED MUX: %s%d, %s%s%d, %s%s%d\n", 
                index(g), index(h),
                type(x) == gtype_Inp ? "$" : "@", index(gate(x)), 
                sign(y)?"-":"", type(y) == gtype_Inp ? "$" : "@", index(gate(y)), 
                sign(z)?"-":"", type(z) == gtype_Inp ? "$" : "@", index(gate(z))
                );
    }
    */

    return is_mux;
    
}

bool matchMux(const Circ& c, Gate g, Sig& x, Sig& y, Sig& z)
{
    if (type(g) != gtype_And) return false;

    Sig left  = c.lchild(g);
    Sig right = c.rchild(g);

    if (!sign(left) || !sign(right) || type(left) != gtype_And || type(right) != gtype_And || c.nFanouts(gate(left)) != 1 || c.nFanouts(gate(right)) != 1) return false;

    return matchMuxParts(c, gate(left), gate(right), x, y, z);
}


bool matchXor(const Circ& c, Gate g, Sig& x, Sig& y)
{
    Sig z;

    if (!matchMux(c, g, x, y, z) || y != ~z) return false;

    y = ~y;

    return true;
}


// NOTE: Not sure what to do about sharing within an xor expression. Just match trees for now.
bool matchXors(const Circ& c, Gate g, vec<Sig>& tmp_stack, vec<Sig>& xs)
{

    Sig x, y;
    if (!matchXor(c, g, x, y)) return false;

    assert(!sign(x));
    bool pol = sign(y);
    y = mkSig(gate(y));

    tmp_stack.clear();
    tmp_stack.push(x);
    tmp_stack.push(y);

    while (tmp_stack.size() > 0){
        Sig sig = tmp_stack.last(); tmp_stack.pop();
        assert(!sign(sig));

        //if (matchXor(c, gate(sig), x, y))
        //fprintf(stderr, "nFanouts = %d\n", c.nFanouts(gate(sig)));

        if (c.nFanouts(gate(sig)) != 2 || !matchXor(c, gate(sig), x, y))
            xs.push(sig);
        else {
            pol = pol ^ sign(y);
            y   = mkSig(gate(y));
            tmp_stack.push(x);
            tmp_stack.push(y);
        }
    }
    assert(xs.size() > 0);
    xs.last() = xs.last() ^ pol;
    normalizeXors(xs);

    // fprintf(stderr, "Matched an Xor chain: ");
    // for (int i = 0; i < xs.size(); i++)
    //     fprintf(stderr, "%s%d ", sign(xs[i])?"-":"", index(gate(xs[i])));
    // fprintf(stderr, "\n");

    return true;
}


void matchAnds(const Circ& c, Gate g, GSet& tmp_set, vec<Sig>& tmp_stack, GMap<int>& tmp_fanouts, vec<Sig>& xs, bool match_muxes)
{
    c.adjustMapSize(tmp_fanouts, 0);
    tmp_set.clear();
    tmp_set.insert(g);
    tmp_stack.clear();
    tmp_stack.push(c.lchild(g));
    tmp_stack.push(c.rchild(g));

    int queue_head = 0;

    while (queue_head < tmp_stack.size()){
        Sig x = tmp_stack[queue_head++];

        Sig tmp_x, tmp_y, tmp_z;
        if (type(x) != gtype_And || sign(x) || !match_muxes && matchMux(c, gate(x), tmp_x, tmp_y, tmp_z))
            continue;

        g = gate(x);
        if (tmp_fanouts[g] < 255)
            tmp_fanouts[g]++;

        if (tmp_fanouts[g] < c.nFanouts(g))
            continue;

        tmp_set.insert(g);
        tmp_stack.push(c.lchild(g));
        tmp_stack.push(c.rchild(g));
    }

    // Clear tmp_fanouts:
    for (int i = 0; i < tmp_stack.size(); i++)
        tmp_fanouts[gate(tmp_stack[i])] = 0;

    // Debug:
    // for (Gate i = c.firstGate(); i != gate_Undef; i = c.nextGate(i))
    //     assert(tmp_fanouts[g] == 0);

    // Add nodes from the 'fringe' of the MFFC:
    xs.clear();
    for (int i = 0; i < tmp_set.size(); i++){
        assert(type(tmp_set[i]) == gtype_And);

        if (!tmp_set.has(gate(c.lchild(tmp_set[i])))) xs.push(c.lchild(tmp_set[i]));
        if (!tmp_set.has(gate(c.rchild(tmp_set[i])))) xs.push(c.rchild(tmp_set[i]));
    }

    normalizeAnds(xs);

    // fprintf(stderr, "Matched a Big-and: ");
    // for (int i = 0; i < xs.size(); i++)
    //     fprintf(stderr, "%s%d ", sign(xs[i])?"-":"", index(gate(xs[i])));
    // fprintf(stderr, "\n");
}



void circInfo(Circ& c, Gate g, GSet& reachable, int& n_ands, int& n_xors, int& n_muxes, int& tot_ands)
{
    if (reachable.has(g) || g == gate_True) return;

    reachable.insert(g);

    Sig x, y, z;

    vec<Sig> xs; xs.clear();

    if (c.matchXors(g, xs)){
        n_xors++;
        for (int i = 0; i < xs.size(); i++)
            circInfo(c, gate(xs[i]), reachable, n_ands, n_xors, n_muxes, tot_ands);
    }else if (c.matchMux(g, x, y, z)){
        n_muxes++;
        circInfo(c, gate(x), reachable, n_ands, n_xors, n_muxes, tot_ands);
        circInfo(c, gate(y), reachable, n_ands, n_xors, n_muxes, tot_ands);
        circInfo(c, gate(z), reachable, n_ands, n_xors, n_muxes, tot_ands);
    }else if (type(g) == gtype_And){
        n_ands++;
        c.matchAnds(g, xs);
        for (int i = 0; i < xs.size(); i++)
            circInfo(c, gate(xs[i]), reachable, n_ands, n_xors, n_muxes, tot_ands);
        tot_ands += xs.size();
    }
}


//=================================================================================================
// Copy the fan-in of signals, from one circuit to another:
//

static        Sig _copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map);
static inline Sig _copySig (const Circ& src, Circ& dst, Sig  x, GMap<Sig>& copy_map){ return _copyGate(src, dst, gate(x), copy_map) ^ sign(x); }
static        Sig _copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map)
{
    if (copy_map[g] == sig_Undef)
        if (type(g) == gtype_Inp)
            copy_map[g] = dst.mkInp();
        else 
            copy_map[g] = dst.mkAnd(_copySig(src, dst, src.lchild(g), copy_map), 
                                    _copySig(src, dst, src.rchild(g), copy_map));

    return copy_map[g];
}


Sig  copyGate(const Circ& src, Circ& dst, Gate g, GMap<Sig>& copy_map) { 
    src.adjustMapSize(copy_map, sig_Undef); return _copyGate(src, dst, g, copy_map); }
Sig  copySig (const Circ& src, Circ& dst, Sig  x, GMap<Sig>& copy_map) {
    src.adjustMapSize(copy_map, sig_Undef); return _copySig (src, dst, x, copy_map); }
void copySig (const Circ& src, Circ& dst, const vec<Sig>& xs, GMap<Sig>& copy_map)
{
    src.adjustMapSize(copy_map, sig_Undef); 
    for (int i = 0; i < xs.size(); i++)
        _copySig(src, dst, xs[i], copy_map);
}
