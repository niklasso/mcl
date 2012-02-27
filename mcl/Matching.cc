/*************************************************************************************[Matching.cc]
Copyright (c) 2007-2011, Niklas Sorensson

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

#include "mcl/Circ.h"
#include "mcl/Matching.h"

namespace Minisat {

//=================================================================================================
// Circuit pattern matching functions:
//

bool CircMatcher::matchMuxParts(const Circ& c, Gate g, Gate h, Sig& x, Sig& y, Sig& z)
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

    return is_mux;
}


bool CircMatcher::matchMux(const Circ& c, Gate g, Sig& x, Sig& y, Sig& z)
{
    if (type(g) != gtype_And) return false;

    Sig left  = c.lchild(g);
    Sig right = c.rchild(g);

    if (!sign(left) || !sign(right) || type(left) != gtype_And || type(right) != gtype_And || c.nFanouts(gate(left)) != 1 || c.nFanouts(gate(right)) != 1) return false;

    return matchMuxParts(c, gate(left), gate(right), x, y, z);
}


bool CircMatcher::matchXor(const Circ& c, Gate g, Sig& x, Sig& y)
{
    Sig z;

    if (!matchMux(c, g, x, y, z) || y != ~z) return false;

    y = ~y;

    // fprintf(stderr, " >>> NODE (%d) MATCHED XOR: %s%s%d, %s%s%d\n", 
    //         index(g), 
    //         sign(x)?"-":"", type(x) == gtype_Inp ? "$" : "@", index(gate(x)), 
    //         sign(y)?"-":"", type(y) == gtype_Inp ? "$" : "@", index(gate(y))
    //         );

    return true;
}


// NOTE: Not sure what to do about sharing within an xor expression. Just match trees for now.
bool CircMatcher::matchXors(const Circ& c, Gate g, vec<Sig>& xs)
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

    // if (xs.size() == 2 && matchXor(c, gate(xs[0]), x, y))
    //     fprintf(stderr, " >>> OOPS! %s%s%d\\%d\n", sign(xs[0])?"-":"", type(xs[0]) == gtype_Inp ? "$":"@", index(gate(xs[0])), c.nFanouts(gate(xs[0])));
    // if (xs.size() == 2 && matchXor(c, gate(xs[1]), x, y))
    //     fprintf(stderr, " >>> OOPS! %s%s%d\\%d\n", sign(xs[1])?"-":"", type(xs[1]) == gtype_Inp ? "$":"@", index(gate(xs[1])), c.nFanouts(gate(xs[1])));
    // 
    // if (xs.size() > 2){
    //     fprintf(stderr, "Matched an Xor chain: ");
    //     for (int i = 0; i < xs.size(); i++)
    //         fprintf(stderr, "%s%d ", sign(xs[i])?"-":"", index(gate(xs[i])));
    //     fprintf(stderr, "\n");
    // }

    return true;
}

void CircMatcher::matchAnds(const Circ& c, Gate g, vec<Sig>& xs, bool match_muxes)
{
    assert(g != gate_Undef);
    assert(g != gate_True);
    assert(type(g) == gtype_And);

    tmp_fanouts.growTo(c.lastGate(), 0);
    tmp_set.clear();
    tmp_set.insert(g);
    tmp_stack.clear();
    tmp_stack.push(c.lchild(g));
    tmp_stack.push(c.rchild(g));

    int queue_head = 0;
    while (queue_head < tmp_stack.size()){
        Sig x = tmp_stack[queue_head++];

        // fprintf(stderr, " >>> matching %d: %s%s%d\\[%d, %d]\n", queue_head,
        //         sign(x)?"-":"", type(x) == gtype_Inp ? "$":"@", 
        //         index(gate(x)), 
        //         tmp_fanouts[gate(x)],
        //         c.nFanouts(gate(x))
        //         );

        assert(gate(x) != gate_Undef);
        assert(gate(x) != gate_True);

        Sig tmp_x, tmp_y, tmp_z;
        // if (type(x) != gtype_And || sign(x) || !match_muxes && matchMux(c, gate(x), tmp_x, tmp_y, tmp_z))
        // if (type(x) != gtype_And || sign(x) || isPinned(gate(x)) || c.nFanouts(gate(x)) > 1 || !match_muxes && matchMux(c, gate(x), tmp_x, tmp_y, tmp_z))
        if (type(x) != gtype_And || sign(x) || isPinned(gate(x)) || 
            (!match_muxes && matchMux(c, gate(x), tmp_x, tmp_y, tmp_z)))
            continue;

        g = gate(x);
        if (tmp_fanouts[g] < 255)
            tmp_fanouts[g]++;
        
        if (tmp_fanouts[g] < c.nFanouts(g))
            continue;

        // fprintf(stderr, " >>> recurse !!\n");

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

    // static int total_size = 0; total_size += xs.size()-1;
    // fprintf(stderr, "total_size = %d\n", total_size);

    // fprintf(stderr, "Matched a Big-and: ");
    // for (int i = 0; i < xs.size(); i++)
    //     fprintf(stderr, "%s%d ", sign(xs[i])?"-":"", index(gate(xs[i])));
    // fprintf(stderr, "\n");
}

void CircMatcher::matchTwoLevel(const Circ& c, Gate g, vec<vec<Sig> >& xss, bool match_muxes)
{
    vec<Sig> top;
    matchAnds(c, g, top, match_muxes);

    xss.clear();
    if (top.size() == 1 && top[0] == sig_False) {
        // Handle constant false:
        xss.push();
        return; }

    for (int i = 0; i < top.size(); i++){
        xss.push();

        if (type(top[i]) == gtype_Inp || !sign(top[i]) || c.nFanouts(gate(top[i])) > 1)
            xss.last().push(top[i]);
        else {
            assert(type(top[i]) == gtype_And);
            matchAnds(c, gate(top[i]), xss.last(), match_muxes);
            for (int j = 0; j < xss.last().size(); j++)
                xss.last()[j] = ~xss.last()[j];
        }
    }

    // fprintf(stderr, " --- MATCHED (%s%d):\n", type(g) == gtype_Inp ? "$" : "@", index(g));
    // for (int i = 0; i < xss.size(); i++){
    //     fprintf(stderr, " --- (%s%s%d)", sign(top[i]) ? "-": "", type(top[i]) == gtype_Inp ? "$" : "@", index(gate(top[i])));
    //     printSigs(xss[i]);
    //     fprintf(stderr, "\n");
    // }

    // vec<vec<Sig> > xss_before;
    // for (int i = 0; i < xss.size(); i++){
    //     xss_before.push();
    //     xss[i].copyTo(xss_before.last());
    // }
    // int size_before = 0;
    // for (int i = 0; i < xss.size(); i++)
    //     size_before += xss[i].size();
            
    vec<vec<Sig> > tmp_context;

    for (int i = 0; i < top.size(); i++)
        if (type(top[i]) == gtype_And && sign(top[i]) && c.nFanouts(gate(top[i])) > 1){
            tmp_context.push();
            matchAnds(c, gate(top[i]), tmp_context.last(), match_muxes);
            for (int j = 0; j < tmp_context.last().size(); j++)
                tmp_context.last()[j] = ~tmp_context.last()[j];
        }

    normalizeTwoLevel(xss, tmp_context);

    // int size_after = 0;
    // for (int i = 0; i < xss.size(); i++)
    //     size_after += xss[i].size();
    // 
    // 
    // if (size_after != size_before){
    //     fprintf(stderr, " <<< gate @%d with %d fanouts matched Two-level:\n", index(g), c.nFanouts(g));
    //     for (int i = 0; i < xss_before.size(); i++){
    //         fprintf(stderr, " >>> "); printSigs(c, xss_before[i]); fprintf(stderr, "\n");
    //     }
    //     
    //     fprintf(stderr, " <<< gate @%d with %d fanouts normalized to:\n", index(g), c.nFanouts(g));
    //     for (int i = 0; i < xss.size(); i++){
    //         fprintf(stderr, " >>> "); printSigs(c, xss[i]); fprintf(stderr, "\n");
    //     }
    // }
    

}

} // namespace Minisat
