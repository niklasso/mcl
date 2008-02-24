/*****************************************************************************************[Aiger.C]
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

#include "utils/ParseUtils.h"
#include "circ/Aiger.h"

using namespace Minisat;

static unsigned int readPacked(StreamBuffer& in){
    unsigned int x = 0, i = 0;
    int ch;

    while ((ch = *in) & 0x80){
        if (ch == EOF) fprintf(stderr, "ERROR! Unexpected end of file!\n"), exit(1);
        x |= (ch & 0x7f) << (7 * i++);
        ++in;
    }
    if (ch == EOF) fprintf(stderr, "ERROR! Unexpected end of file!\n"), exit(1);
    ++in;
    
    return x | (ch << (7 * i));
}


static Sig aigToSig(vec<Sig>& id2sig, int aiger_lit) { 
    if (aiger_lit == 0)      return sig_False;
    else if (aiger_lit == 1) return sig_True;
    else return id2sig[aiger_lit >> 1] ^ bool(aiger_lit & 1);
}


void Minisat::readAiger(const char* filename, AigerCirc& c)
{
    gzFile f = gzopen(filename, "rb");

    if (f == NULL)
        fprintf(stderr, "ERROR! Could not open file <%s> for reading\n", filename), exit(1);

    StreamBuffer in(f);

    if (!eagerMatch(in, "aig "))
        fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *in), exit(1);

    int max_var   = parseInt(in);
    int n_inputs  = parseInt(in);
    int n_latches = parseInt(in);
    int n_outputs = parseInt(in);
    int n_gates   = parseInt(in);

    // fprintf(stderr, "max_var   = %d\n", max_var);
    // fprintf(stderr, "n_inputs  = %d\n", n_inputs);
    // fprintf(stderr, "n_outputs = %d\n", n_outputs);
    // fprintf(stderr, "n_latches = %d\n", n_latches);
    // fprintf(stderr, "n_gates   = %d\n", n_gates);
    // fprintf(stderr, "sum       = %d\n", n_gates + n_latches + n_inputs);

    if (max_var != n_inputs + n_latches + n_gates)
        fprintf(stderr, "ERROR! Header mismatching sizes (M != I + L + A)\n"), exit(1);

    c.circ.clear();
    c.inputs.clear();
    c.latches.clear();
    c.outputs.clear();
    c.latch_defs.clear();

    vec<Sig> id2sig(max_var, sig_Undef);

    // Create input gates:
    for (int i = 0; i < n_inputs; i++){
        Sig x = c.circ.mkInp();
        c.inputs.push(gate(x));
        id2sig[i+1] = x;
    }

    // Create latch gates:
    for (int i = 0; i < n_latches; i++){
        Sig x = c.circ.mkInp();
        c.latches.push(gate(x));
        id2sig[i+n_inputs+1] = x;
    }

    vec<unsigned int> aiger_outputs;
    vec<unsigned int> aiger_latch_defs;

    // Read latch definitions:
    for (int i = 0; i < n_latches; i++){
        aiger_latch_defs.push(parseInt(in));
        skipLine(in); }

    // Read outputs:
    for (int i = 0; i < n_outputs; i++){
        aiger_outputs.push(parseInt(in));
        skipLine(in); }

    // Read gates:
    for (int i = n_inputs + n_latches + 1; i < max_var + 1; i++){
        unsigned delta0 = readPacked(in);
        unsigned delta1 = readPacked(in);
        unsigned x      = 2*i - delta0;
        unsigned y      = x   - delta1;

        // fprintf(stderr, "read gate %d = %d & %d\n", 2*i, x, y);

        id2sig[i]       = c.circ.mkAnd(aigToSig(id2sig, x), aigToSig(id2sig, y));

        assert   ((int)delta0 < 2*i);
        assert   (delta1 <= 2*i - delta0);
    }
    // printf("Read %d number of gates\n", c.nGates());

    // Map outputs:
    for (int i = 0; i < aiger_outputs.size(); i++)
        c.outputs.push(aigToSig(id2sig, aiger_outputs[i]));

    // Map latches:
    c.circ.adjustMapSize(c.latch_defs, sig_Undef);
    for (int i = 0; i < aiger_latch_defs.size(); i++)
        c.latch_defs[c.latches[i]] = aigToSig(id2sig, aiger_latch_defs[i]);

    gzclose(f);
}

static void writePacked(FILE* file, unsigned int x)
{
    unsigned char ch;
    
    while (x & ~0x7f){
        ch = (x & 0x7f) | 0x80;
        putc(ch, file);
        x >>= 7;
    }
     
    ch = x;
    putc(ch, file);
}


static unsigned int sigToAig(GMap<unsigned int>& gate2id, Sig x) { 
    if (x == sig_False)      return 0;
    else if (x == sig_True)  return 1;
    else return (gate2id[gate(x)]<<1) + (unsigned int)sign(x);
}


void Minisat::writeAiger(const char* filename, const AigerCirc& c)
{
    // fprintf(stderr, "aig %u %u %u %d %d\n", c.size(), inputs.size(), latch_defs.size(), outputs.size(), c.nGates());

    // Generate the set of reachable gates:
    // GSet reachable; 
    // bottomUpOrder(c.circ, c.outputs, reachable); 
    // bottomUpOrder(c.circ, c.latches, c.latch_defs, reachable);

    unsigned int n_inputs = 0, n_latches = 0;

    // Generate the same set, but in an order compatible with the AIGER format:
    GSet uporder;
    for (int i = 0; i < c.inputs.size(); i++)
        //if (reachable.has(c.inputs[i]))
            n_inputs++, uporder.insert(c.inputs[i]);

    vec<Gate> ls;
    for (int i = 0; i < c.latches.size(); i++)
        // if (reachable.has(c.latches[i]))
        {
            n_latches++; 
            ls.push(c.latches[i]);
            uporder.insert(c.latches[i]);
        }

    bottomUpOrder(c.circ, c.outputs, uporder);
    bottomUpOrder(c.circ, c.latches, c.latch_defs, uporder);

    unsigned int n_gates = uporder.size() - n_inputs - n_latches;

    GMap<unsigned int> gate2id; c.circ.adjustMapSize(gate2id);
    for (int i = 0; i < uporder.size(); i++)
        gate2id[uporder[i]] = i + 1;

    FILE* f = fopen(filename, "wb");

    if (f == NULL)
        fprintf(stderr, "ERROR! Could not open file <%s> for writing\n", filename), exit(1);

    // fprintf(stderr, "aig %u %u %u %d %d\n", uporder.size(), n_inputs, n_latches, c.outputs.size(), n_gates);
    fprintf(f, "aig %u %u %u %d %d\n", uporder.size(), n_inputs, n_latches, c.outputs.size(), n_gates);

    // Write latch-defs:
    for (int i = 0; i < ls.size(); i++)
        fprintf(f, "%u\n", sigToAig(gate2id, c.latch_defs[ls[i]]));

    // Write outputs:
    for (int i = 0; i < c.outputs.size(); i++)
        fprintf(f, "%u\n", sigToAig(gate2id, c.outputs[i]));

    // Write gates:
    for (int i = 0; i < (int)n_gates; i++){
        Gate         g    = uporder[n_inputs + n_latches + i];     assert(type(g) == gtype_And);
        unsigned int glit = (n_inputs + n_latches + i + 1) << 1;
        unsigned int llit = sigToAig(gate2id, c.circ.lchild(g));
        unsigned int rlit = sigToAig(gate2id, c.circ.rchild(g));

        if (llit < rlit){
            unsigned int tmp = llit; llit = rlit; rlit = tmp; }

        assert(glit > llit);
        assert(llit >= rlit);

        // fprintf(stderr, "%d = %d & %d\n", glit, llit, rlit);
        writePacked(f, glit - llit);
        writePacked(f, llit - rlit);
    }
    fclose(f);
}
