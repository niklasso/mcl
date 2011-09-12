/******************************************************************************************[Smv.cc]
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

#include "mcl/Matching.h"
#include "mcl/Smv.h"

using namespace Minisat;

static void writeSmvSig(FILE* f, Sig x)
{
    if (x == sig_True)
        fprintf(f, "1");
    else if (x == sig_False)
        fprintf(f, "0");
    else
        fprintf(f, "%s%c%03d", sign(x)?"!":"", type(x) == gtype_Inp ? 'x' : 'g', index(gate(x)));
}

static void writeSmvWithOp(FILE* f, const char* op, vec<Sig>& xs)
{
    writeSmvSig(f, xs[0]);
    for (int i = 1; i < xs.size(); i++){
        fprintf(f, " %s ", op);
        writeSmvSig(f, xs[i]);
    }
}

static void recursiveWriteSmv(FILE* f, Circ& c, CircMatcher& cm, Gate g, GSet& reached, bool structured)
{
    if (reached.has(g)) return;
    reached.insert(g);

    if (type(g) == gtype_And){
        Sig x, y, z;
        vec<Sig> xs;

        if (!structured){
            x = c.lchild(g);
            y = c.rchild(g);
            recursiveWriteSmv(f, c, cm, gate(x), reached, structured);
            recursiveWriteSmv(f, c, cm, gate(y), reached, structured);
            writeSmvSig(f, mkSig(g));
            fprintf(f, " := ");
            writeSmvSig(f, x);
            fprintf(f, " & ");
            writeSmvSig(f, y);
            fprintf(f, ";\n");
        }else if (cm.matchXors(c, g, xs)){
            for (int i = 0; i < xs.size(); i++)
                recursiveWriteSmv(f, c, cm, gate(xs[i]), reached, structured);
            writeSmvSig(f, mkSig(g));
            fprintf(f, " := ");
            writeSmvWithOp(f, "^", xs);
            fprintf(f, ";\n");
        }else if (cm.matchMux(c, g, x, y, z)){
            recursiveWriteSmv(f, c, cm, gate(x), reached, structured);
            recursiveWriteSmv(f, c, cm, gate(y), reached, structured);
            recursiveWriteSmv(f, c, cm, gate(z), reached, structured);
            writeSmvSig(f, mkSig(g));
            fprintf(f, " := ");
            writeSmvSig(f, x);
            fprintf(f, " ? ");
            writeSmvSig(f, y);
            fprintf(f, " : ");
            writeSmvSig(f, z);
            fprintf(f, ";\n");
        }else{
            cm.matchAnds(c, g, xs);
            for (int i = 0; i < xs.size(); i++)
                recursiveWriteSmv(f, c, cm, gate(xs[i]), reached, structured);

            writeSmvSig(f, mkSig(g));
            fprintf(f, " := ");
            writeSmvWithOp(f, "&", xs);
            fprintf(f, ";\n");
        }
    }
}

// TODO: handle initialized flops?!
void Minisat::writeSmv(const char* filename, Circ& c, const Box& b, const Flops& flp, 
                       bool structured)
{
    FILE* f = fopen(filename, "wb");

    if (f == NULL)
        fprintf(stderr, "ERROR! Could not open file <%s> for writing\n", filename), exit(1);

    fprintf(f, "MODULE main\n");
    fprintf(f, "VAR\n");
    fprintf(f, "--inputs\n");
    for (int i = 0; i < b.inps.size(); i++){
        writeSmvSig(f, mkSig(b.inps[i]));
        fprintf(f, " : boolean;\n");
    }
    fprintf(f, "--flops\n");
    for (int i = 0; i < flp.size(); i++){
        writeSmvSig(f, mkSig(flp[i]));
        fprintf(f, " : boolean;\n");
    }

    fprintf(f, "ASSIGN\n");
    for (int i = 0; i < flp.size(); i++){
        Gate g = flp[i];
        Sig  d = flp.next(flp[i]);
        // Check that flops are zero-initialized:
        assert(flp.init(flp[i]) == sig_False);
        fprintf(f, "init(");
        writeSmvSig(f, mkSig(g));
        fprintf(f, ") := 0;\n");

        fprintf(f, "next(");
        writeSmvSig(f, mkSig(g));
        fprintf(f, ") := ");
        writeSmvSig(f, d);
        fprintf(f, ";\n");
    }

    fprintf(f, "DEFINE\n");
    GSet        reached;
    vec<Sig>    bads;
    CircMatcher cm;
    for (int i = 0; i < b.outs.size(); i++){
        bads.push(~b.outs[i]);
        recursiveWriteSmv(f, c, cm, gate(b.outs[i]), reached, structured);
    }
    for (int i = 0; i < flp.size(); i++)
        recursiveWriteSmv(f, c, cm, gate(flp.next(flp[i])), reached, structured);

    fprintf(f, "__prop := ");
    writeSmvWithOp(f, "|", bads);
    fprintf(f, ";\n");

    fprintf(f, "SPEC AG __prop\n");

    fclose(f);
}
