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
    else {
      assert((aiger_lit >> 1) < id2sig.size());
      assert((aiger_lit >> 1) >= 0);
      return id2sig[aiger_lit >> 1] ^ bool(aiger_lit & 1);
    }
}


void Minisat::readAiger(const char* filename, Circ& c, Box& b, Flops& flp)
{
    gzFile f = gzopen(filename, "rb");

    if (f == NULL)
        fprintf(stderr, "ERROR! Could not open file <%s> for reading\n", filename), exit(1);

    StreamBuffer in(f);

    if (!eagerMatch(in, "aig "))
        fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *in), exit(1);

    int max_var   = parseInt(in);
    int n_inputs  = parseInt(in);
    int n_flops = parseInt(in);
    int n_outputs = parseInt(in);
    int n_gates   = parseInt(in);

    // fprintf(stderr, "max_var   = %d\n", max_var);
    // fprintf(stderr, "n_inputs  = %d\n", n_inputs);
    // fprintf(stderr, "n_outputs = %d\n", n_outputs);
    // fprintf(stderr, "n_flops = %d\n", n_flops);
    // fprintf(stderr, "n_gates   = %d\n", n_gates);
    // fprintf(stderr, "sum       = %d\n", n_gates + n_flops + n_inputs);

    if (max_var != n_inputs + n_flops + n_gates)
        fprintf(stderr, "ERROR! Header mismatching sizes (M != I + L + A)\n"), exit(1);

    c.clear();
    b.clear();
    flp.clear();

    vec<Sig> id2sig(max_var+1, sig_Undef);

    // Create input gates:
    for (int i = 0; i < n_inputs; i++){
        Sig x = c.mkInp();
        b.inps.push(gate(x));
        id2sig[i+1] = x;
    }

    vec<unsigned int> aiger_outputs;
    vec<unsigned int> aiger_latch_defs;
    vec<Gate>         latch_gates;

    // Create latch gates:
    for (int i = 0; i < n_flops; i++){
        Sig x = c.mkInp();
        id2sig[i+n_inputs+1] = x;
        latch_gates.push(gate(x));
    }

    // Read latch definitions:
    for (int i = 0; i < n_flops; i++){
        aiger_latch_defs.push(parseInt(in));
        skipLine(in); }

    // Read outputs:
    for (int i = 0; i < n_outputs; i++){
        aiger_outputs.push(parseInt(in));
        skipLine(in); }

    // Read gates:
    for (int i = n_inputs + n_flops + 1; i < max_var + 1; i++){
        unsigned delta0 = readPacked(in);
        unsigned delta1 = readPacked(in);
        unsigned x      = 2*i - delta0;
        unsigned y      = x   - delta1;
        id2sig[i]       = c.mkAnd(aigToSig(id2sig, x), aigToSig(id2sig, y));

        assert(i < id2sig.size());
        assert((int)delta0 < 2*i);
        assert(delta1 <= 2*i - delta0);
        // fprintf(stderr, "read gate %d = %d & %d\n", 2*i, x, y);
    }

    // Map outputs:
    for (int i = 0; i < aiger_outputs.size(); i++)
        b.outs.push(aigToSig(id2sig, aiger_outputs[i]));

    // Map flops:
    flp.adjust(c);
    for (int i = 0; i < aiger_latch_defs.size(); i++){
        Sig x = aigToSig(id2sig, aiger_latch_defs[i]);
        flp.defineFlop(latch_gates[i], x);
    }

    gzclose(f);
    // printf("Read %d number of gates\n", c.nGates());
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


void Minisat::writeAiger(const char* filename, const Circ& c, const Box& b, const Flops& flp)
{
    // fprintf(stderr, "aig %u %u %u %d %d\n", c.size(), inputs.size(), latch_defs.size(), outputs.size(), c.nGates());

    unsigned int n_inputs = b.inps.size(), n_flops = flp.size();

    // Generate the same set, but in an order compatible with the AIGER format:
    GSet uporder;
    for (int i = 0; i < b.inps.size(); i++) uporder.insert(b.inps[i]);
    for (int i = 0; i < flp.size(); i++)    uporder.insert(flp[i]);

    // Build set of all sink-nodes:
    vec<Gate> sinks;
    for (int i = 0; i < b.outs.size(); i++) sinks.push(gate(b.outs[i]));
    for (int i = 0; i < flp   .size(); i++) sinks.push(gate(flp.def(flp[i])));
    bottomUpOrder(c, sinks, uporder);

    unsigned int       n_gates = uporder.size() - n_inputs - n_flops;
    GMap<unsigned int> gate2id; c.adjustMapSize(gate2id);
    for (int i = 0; i < uporder.size(); i++)
        gate2id[uporder[i]] = i + 1;

    FILE* f = fopen(filename, "wb");

    if (f == NULL)
        fprintf(stderr, "ERROR! Could not open file <%s> for writing\n", filename), exit(1);

    // fprintf(stderr, "aig %u %u %u %d %d\n", uporder.size(), n_inputs, n_flops, c.outputs.size(), n_gates);
    fprintf(f, "aig %u %u %u %d %d\n", uporder.size(), n_inputs, n_flops, b.outs.size() - n_flops, n_gates);

    // Write latch-defs:
    for (int i = 0; i < flp.size(); i++)
        fprintf(f, "%u\n", sigToAig(gate2id, flp.def(flp[i])));

    // Write outputs:
    for (int i = 0; i < b.outs.size(); i++)
        fprintf(f, "%u\n", sigToAig(gate2id, b.outs[i]));

    // Write gates:
    for (int i = 0; i < (int)n_gates; i++){
        Gate         g    = uporder[n_inputs + n_flops + i];     assert(type(g) == gtype_And);
        unsigned int glit = (n_inputs + n_flops + i + 1) << 1;
        unsigned int llit = sigToAig(gate2id, c.lchild(g));
        unsigned int rlit = sigToAig(gate2id, c.rchild(g));

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
