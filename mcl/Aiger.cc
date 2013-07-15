/****************************************************************************************[Aiger.cc]
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

#include "minisat/utils/ParseUtils.h"
#include "mcl/Aiger.h"
#include "mcl/CircPrelude.h"

using namespace Minisat;

//=================================================================================================
// Basic helpers:
//

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


//=================================================================================================
// Read/Write for AIGER (version 1) circuits:
//


void Minisat::readAiger(const char* filename, SeqCirc& c, vec<Sig>& outs)
{
    gzFile f = gzopen(filename, "rb");

    if (f == NULL)
        fprintf(stderr, "ERROR! Could not open file <%s> for reading\n", filename), exit(1);

    StreamBuffer in(f);

    if (!eagerMatch(in, "aig "))
        fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *in), exit(1);

    int max_var   = parseInt(in);
    int n_inputs  = parseInt(in);
    int n_flops   = parseInt(in);
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
    outs.clear();

    vec<Sig> id2sig(max_var+1, sig_Undef);

    // Create input gates:
    for (int i = 0; i < n_inputs; i++){
        Sig x = c.main.mkInp(i);
        id2sig[i+1] = x;
    }

    vec<unsigned int> aiger_outputs;
    vec<unsigned int> aiger_latch_defs;
    vec<Gate>         latch_gates;

    // Create latch gates:
    for (int i = 0; i < n_flops; i++){
        Sig x = c.main.mkInp(i);
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
        id2sig[i]       = c.main.mkAnd(aigToSig(id2sig, x), aigToSig(id2sig, y));

        assert(i < id2sig.size());
        assert((int)delta0 < 2*i);
        assert(delta1 <= 2*i - delta0);
        // fprintf(stderr, "read gate %d = %d & %d\n", 2*i, x, y);
    }

    // Map outputs:
    for (int i = 0; i < aiger_outputs.size(); i++)
        outs.push(aigToSig(id2sig, aiger_outputs[i]));

    // Map flops:
    for (int i = 0; i < aiger_latch_defs.size(); i++){
        Sig x = aigToSig(id2sig, aiger_latch_defs[i]);
        c.flps.define(latch_gates[i], x, sig_False);
    }

    gzclose(f);
    // printf("Read %d number of gates\n", c.main.nGates());
}

// PRECONDITION: Primary inputs of the circuit must have a unique numbering {0...n} without any
// holes.
void Minisat::writeAiger(const char* filename, const SeqCirc& c, const vec<Sig>& outs)
{
    // fprintf(stderr, "aig %u %u %u %d %d\n", c.main.size(), inputs.size(), latch_defs.size(), outputs.size(), c.main.nGates());

    // Generate list of inputs ordered by input numbering:
#if 0
    vec<Gate> inps;
    for (InpIt iit = c.inpBegin(); iit != c.inpEnd(); ++iit){
        Gate     inp = *iit;
        uint32_t num = c.main.number(inp);

        assert(num != UINT32_MAX);       // Check that inputs has a numbering that is somewhat sane.
        assert(num < (uint32_t)c.main.size());
        inps.growTo(num+1, gate_Undef);
        assert(inps[num] == gate_Undef); // Check that this number has not been used before.
        inps[num] = inp;
    }

#ifndef NDEBUG
    // Check that numbering does not contain holes.
    for (int i = 0; i < inps.size(); i++)
        assert(inps[i] != gate_Undef);
#endif
#else
    vec<Gate> inps;
    for (SeqCirc::InpIt iit = c.inpBegin(); iit != c.inpEnd(); ++iit)
        inps.push(*iit);
#endif

    uint32_t n_inputs = inps.size();
    uint32_t n_flops  = c.flps.size();

    // Generate the same set, but in an order compatible with the AIGER format:
    GSet uporder;
    for (int i = 0; i < inps.size(); i++)   uporder.insert(inps[i]);
    for (int i = 0; i < c.flps.size(); i++) uporder.insert(c.flps[i]);

    // Build set of all sink-nodes:
    vec<Gate> sinks;
    for (int i = 0; i < outs.size(); i++)   sinks.push(gate(outs[i]));
    for (int i = 0; i < c.flps.size(); i++) sinks.push(gate(c.flps.next(c.flps[i])));

    // AIGER only supports zero-initialized flops, and there is no conversion built into this
    // function. TODO: make this check always-on somehow.
    for (SeqCirc::FlopIt fit = c.flpsBegin(); fit != c.flpsEnd(); ++fit)
        if (c.flps.init(*fit) != sig_False)
            printf("ERROR! AIGER writer only supports zero-initialized flops at the moment.\n"), exit(1);

    bottomUpOrder(c.main, sinks, uporder);

    uint32_t           n_gates = uporder.size() - n_inputs - n_flops;
    GMap<unsigned int> gate2id; gate2id.growTo(c.main.lastGate());
    for (int i = 0; i < uporder.size(); i++)
        gate2id[uporder[i]] = i + 1;

    FILE* f = fopen(filename, "wb");

    if (f == NULL)
        fprintf(stderr, "ERROR! Could not open file <%s> for writing\n", filename), exit(1);

    // fprintf(stderr, "aig %u %u %u %d %d\n", uporder.size(), n_inputs, n_flops, c.outputs.size(), n_gates);
    fprintf(f, "aig %u %u %u %d %d\n", uporder.size(), n_inputs, n_flops, outs.size(), n_gates);

    // Write latch-defs:
    for (int i = 0; i < c.flps.size(); i++)
        fprintf(f, "%u\n", sigToAig(gate2id, c.flps.next(c.flps[i])));

    // Write outputs:
    for (int i = 0; i < outs.size(); i++)
        fprintf(f, "%u\n", sigToAig(gate2id, outs[i]));

    // Write gates:
    for (int i = 0; i < (int)n_gates; i++){
        Gate         g    = uporder[n_inputs + n_flops + i];     assert(type(g) == gtype_And);
        unsigned int glit = (n_inputs + n_flops + i + 1) << 1;
        unsigned int llit = sigToAig(gate2id, c.main.lchild(g));
        unsigned int rlit = sigToAig(gate2id, c.main.rchild(g));

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


void Minisat::readAiger(const char* filename, Circ& c, vec<Sig>& outs)
{
    SeqCirc tmp;
    readAiger(filename, tmp, outs);
    tmp.main.moveTo(c);
    printf("WARNING! Sequential circuit truncated to combinational during AIGER read.\n");
}


void Minisat::writeAiger(const char* filename, const Circ& c, const vec<Sig>& outs)
{
    SeqCirc tmp;

    // TODO: implement this?
    // c.copyTo(tmp.main);
    exit(0);

    printf("WARNING! Sequential circuit truncated to combinational during AIGER write.\n");
    writeAiger(filename, tmp, outs);
}


//=================================================================================================
// Read/Write for AIGER (version 1.9) circuits:
//

void Minisat::readAiger_v19(const char* filename, SeqCirc& c, AigerSections& sects)
{
    gzFile f = gzopen(filename, "rb");

    if (f == NULL)
        fprintf(stderr, "ERROR! Could not open file <%s> for reading\n", filename), exit(1);

    StreamBuffer in(f);

    if (!eagerMatch(in, "aig "))
        fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *in), exit(1);

    vec<int> header(9, 0);
    for (int i = 0; *in != '\n'; i++)
        if (i < 9)
            header[i] = parseInt(in);
        else
            fprintf(stderr, "ERROR! Header contains too many sections\n"), exit(1);
    skipLine(in);

    int max_var   = header[0];
    int n_inputs  = header[1];
    int n_flops   = header[2];
    int n_outputs = header[3];
    int n_gates   = header[4];

    int n_bads    = header[5];
    int n_cnstrs  = header[6];
    int n_justs   = header[7];
    int n_fairs   = header[8];

    fprintf(stdout, "max_var   = %d\n", max_var);
    fprintf(stdout, "n_inputs  = %d\n", n_inputs);
    fprintf(stdout, "n_outputs = %d\n", n_outputs);
    fprintf(stdout, "n_flops   = %d\n", n_flops);
    fprintf(stdout, "n_gates   = %d\n", n_gates);
    fprintf(stdout, "n_bads    = %d\n", n_bads);
    fprintf(stdout, "n_cnstrs  = %d\n", n_cnstrs);
    fprintf(stdout, "n_justs   = %d\n", n_justs);
    fprintf(stdout, "n_fairs   = %d\n", n_fairs);
    fprintf(stdout, "sum       = %d\n", n_gates + n_flops + n_inputs);

    if (max_var != n_inputs + n_flops + n_gates)
        fprintf(stderr, "ERROR! Header mismatching sizes (M != I + L + A)\n"), exit(1);

    c           .clear();
    sects.outs  .clear();
    sects.cnstrs.clear();
    sects.fairs .clear();
    sects.bads  .clear();
    sects.justs .clear();

    vec<Sig> id2sig(max_var+1, sig_Undef);

    // Create input gates:
    for (int i = 0; i < n_inputs; i++){
        Sig x = c.main.mkInp(i);
        id2sig[i+1] = x;
    }

    vec<unsigned int> aiger_outputs;
    vec<unsigned int> aiger_bads;
    vec<unsigned int> aiger_cnstrs;
    vec<vec<unsigned int> > aiger_justs;
    vec<unsigned int> aiger_fairs;
    vec<unsigned int> aiger_latch_nexts;
    vec<unsigned int> aiger_latch_inits;
    vec<Gate>         latch_gates;

    // Create latch gates:
    for (int i = 0; i < n_flops; i++){
        Sig x = c.main.mkInp(i);
        id2sig[i+n_inputs+1] = x;
        latch_gates.push(gate(x));
    }

    // Read latch definitions:
    for (int i = 0; i < n_flops; i++){
        aiger_latch_nexts.push(parseInt(in));
        if (*in != '\n')
            aiger_latch_inits.push(parseInt(in));
        else
            aiger_latch_inits.push(0);
        skipLine(in);
    }

    // Read outputs:
    for (int i = 0; i < n_outputs; i++){
        aiger_outputs.push(parseInt(in));
        skipLine(in); }

    // Read bads:
    for (int i = 0; i < n_bads; i++){
        aiger_bads.push(parseInt(in));
        skipLine(in); }

    // Read constraints:
    for (int i = 0; i < n_cnstrs; i++){
        aiger_cnstrs.push(parseInt(in));
        skipLine(in); }

    // Read justice properties:
    for (int i = 0; i < n_justs; i++){
        aiger_justs.push();
        aiger_justs.last().growTo(parseInt(in));
        skipLine(in); }
    for (int i = 0; i < aiger_justs.size(); i++)
        for (int j = 0; j < aiger_justs[i].size(); j++){
            aiger_justs[i][j] = parseInt(in);
            skipLine(in); }

    // Read fairness constraints:
    for (int i = 0; i < n_fairs; i++){
        aiger_fairs.push(parseInt(in));
        skipLine(in); }
    
    // Read gates:
    for (int i = n_inputs + n_flops + 1; i < max_var + 1; i++){
        unsigned delta0 = readPacked(in);
        unsigned delta1 = readPacked(in);
        unsigned x      = 2*i - delta0;
        unsigned y      = x   - delta1;
        id2sig[i]       = c.main.mkAnd(aigToSig(id2sig, x), aigToSig(id2sig, y));

        assert(i < id2sig.size());
        assert(delta0 <= 2*i);
        assert(delta1 <= 2*i - delta0);
        // fprintf(stderr, "read gate %d = %d & %d\n", 2*i, x, y);
    }

    // Map outputs:
    for (int i = 0; i < aiger_outputs.size(); i++)
        sects.outs.push(aigToSig(id2sig, aiger_outputs[i]));

    // Map bads:
    for (int i = 0; i < aiger_bads.size(); i++)
        sects.bads.push(aigToSig(id2sig, aiger_bads[i]));

    // Map constraints:
    for (int i = 0; i < aiger_cnstrs.size(); i++)
        sects.cnstrs.push(aigToSig(id2sig, aiger_cnstrs[i]));

    // Map justice properties:
    for (int i = 0; i < aiger_justs.size(); i++){
        sects.justs.push();
        for (int j = 0; j < aiger_justs[i].size(); j++)
            sects.justs.last().push(aigToSig(id2sig, aiger_justs[i][j]));
    }

    // Map fairness constraints:
    for (int i = 0; i < aiger_fairs.size(); i++)
        sects.fairs.push(aigToSig(id2sig, aiger_fairs[i]));

    // Map flops:
    uint32_t init_x_id = 0;
    for (int i = 0; i < aiger_latch_nexts.size(); i++){
        Sig next = aigToSig(id2sig, aiger_latch_nexts[i]);
        Sig init = aigToSig(id2sig, aiger_latch_inits[i]);

        // If init-definition is equal to 
        if (init == mkSig(latch_gates[i])){
            init = c.init.mkInp(init_x_id++);
        }else if (type(init) != gtype_Const)
            fprintf(stderr, "ERROR! Flop initialized to something other than 0/1/X.\n"), exit(1);
            
        c.flps.define(latch_gates[i], next, init);
    }

    gzclose(f);
    // printf("Read %d number of gates\n", c.main.nGates());
}


void Minisat::writeAiger_v19(const char* filename, const SeqCirc& c, const AigerSections& sects)
{
}
