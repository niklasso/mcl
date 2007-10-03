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

#include "ParseUtils.h"
#include "Aiger.h"

unsigned int readPacked(StreamBuffer& in){
    unsigned int x = 0, i = 0;
    int          ch;

    while ((ch = *in) & 0x80){
        if (ch == EOF) fprintf(stderr, "ERROR! Unexpected end of file!\n"), exit(1);
        ++in;
        x |= (ch & 0x7f) << (7 * i++);
    }
    if (ch == EOF) fprintf(stderr, "ERROR! Unexpected end of file!\n"), exit(1);
    
    return x | (ch << (7 * i));
}


void readAiger (const char* filename, Circ& c, vec<Sig>& inputs, vec<Def>& latch_defs, vec<Sig>& outputs)
{
    gzFile f = gzopen(filename, "rb");

    if (f == NULL)
        fprintf(stderr, "ERROR! Could not open file: %s\n", filename), exit(1);

    StreamBuffer in(f);

    if (!match(in, "aig "))
        fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *in), exit(1);

    int max_var   = parseInt(in);
    int n_inputs  = parseInt(in);
    int n_latches = parseInt(in);
    int n_outputs = parseInt(in);
    int n_gates   = parseInt(in);

    fprintf(stderr, "max_var   = %d\n", max_var);
    fprintf(stderr, "n_inputs  = %d\n", n_inputs);
    fprintf(stderr, "n_outputs = %d\n", n_outputs);
    fprintf(stderr, "n_latches = %d\n", n_latches);
    fprintf(stderr, "n_gates   = %d\n", n_gates);
    fprintf(stderr, "sum       = %d\n", n_gates + n_latches + n_inputs);

    if (max_var != n_inputs + n_latches + n_gates)
        fprintf(stderr, "ERROR! Header mismatching sizes (M != I + L + A)\n"), exit(1);

    vec<Gate> id2gate(max_var, gate_Undef);

    // Create input gates:
    for (int i = 0; i < n_inputs; i++){
        Sig x = c.mkInp();
        inputs.push(x);
        id2gate[i+1] = gate(x);
    }

    // Create latch gates:
    for (int i = 0; i < n_latches; i++){
        Def d = { c.mkInp(), sig_Undef };
        latch_defs.push(d);
        id2gate[i+n_inputs+1] = gate(d.var);
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
        unsigned x      = i - delta0;
        unsigned y      = x - delta1;
        //printf("i = %3d, x = %3d, y = %3d\n", i, x, y);
        Sig      left   = x == 0 ? sig_False : x == 1 ? sig_True : mkSig(id2gate[x >> 1], x&1);
        Sig      right  = y == 0 ? sig_False : y == 1 ? sig_True : mkSig(id2gate[y >> 1], y&1);
        id2gate[i]      = gate(c.mkAnd(left, right));
    }
    printf("Resulting number of gates: %d\n", c.nGates());
}


void writeAiger(const char* filename, Circ& c, const vec<Sig>& inputs, const vec<Def>& latch_defs, const vec<Sig>& outputs)
{
}
