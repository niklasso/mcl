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

#include "circ/Smv.h"

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

static void recursiveWriteSmv(FILE* f, Circ& c, Gate g, GSet& reached, bool tip_mode, bool structured)
{
    if (reached.has(g)) return;
    reached.insert(g);

    if (type(g) == gtype_And){
        Sig x, y, z;
        vec<Sig> xs;

        if (!structured){
            x = c.lchild(g);
            y = c.rchild(g);
            recursiveWriteSmv(f, c, gate(x), reached, tip_mode, structured);
            recursiveWriteSmv(f, c, gate(y), reached, tip_mode, structured);
            writeSmvSig(f, mkSig(g));
            fprintf(f, " := ");
            writeSmvSig(f, x);
            fprintf(f, " & ");
            writeSmvSig(f, y);
            fprintf(f, ";\n");
        }else if ( tip_mode && c.matchXor(g, x, y)){
            recursiveWriteSmv(f, c, gate(x), reached, tip_mode, structured);
            recursiveWriteSmv(f, c, gate(y), reached, tip_mode, structured);
            writeSmvSig(f, mkSig(g));
            fprintf(f, " := ");
            //fprintf(f, "x%d := ", index(g));
            writeSmvSig(f, x);
            fprintf(f, tip_mode ? " <-> " : " ^ ");
            writeSmvSig(f, tip_mode ? ~y : y);
            fprintf(f, ";\n");
        }else if (!tip_mode && c.matchXors(g, xs)){
            for (int i = 0; i < xs.size(); i++)
                recursiveWriteSmv(f, c, gate(xs[i]), reached, tip_mode, structured);
            writeSmvSig(f, mkSig(g));
            fprintf(f, " := ");
            //fprintf(f, "x%d := ", index(g));
            writeSmvWithOp(f, "^", xs);
            fprintf(f, ";\n");
        }else if (c.matchMux(g, x, y, z)){
            recursiveWriteSmv(f, c, gate(x), reached, tip_mode, structured);
            recursiveWriteSmv(f, c, gate(y), reached, tip_mode, structured);
            recursiveWriteSmv(f, c, gate(z), reached, tip_mode, structured);
            writeSmvSig(f, mkSig(g));
            fprintf(f, " := ");
            //fprintf(f, "x%d := ", index(g));
            if (tip_mode){
                fprintf(f, "(");
                writeSmvSig(f, x);
                fprintf(f, " & ");
                writeSmvSig(f, y);
                fprintf(f, ") | (");
                writeSmvSig(f, ~x);
                fprintf(f, " & ");
                writeSmvSig(f, z);
                fprintf(f, ");\n");
            }else{
                writeSmvSig(f, x);
                fprintf(f, " ? ");
                writeSmvSig(f, y);
                fprintf(f, " : ");
                writeSmvSig(f, z);
                fprintf(f, ";\n");
            }
        }else{
            c.matchAnds(g, xs);
            for (int i = 0; i < xs.size(); i++)
                recursiveWriteSmv(f, c, gate(xs[i]), reached, tip_mode, structured);

            writeSmvSig(f, mkSig(g));
            fprintf(f, " := ");
            //fprintf(f, "x%d := ", index(g));
            writeSmvWithOp(f, "&", xs);
            fprintf(f, ";\n");
        }
    }
}

void Minisat::writeSmv(const char* filename, Circ& c, const Box& b, const Flops& flp, bool tip_mode, bool structured)
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
        Sig  d = flp.def(flp[i]);
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
    GSet     reached;
    vec<Sig> bads;
    for (int i = 0; i < b.outs.size(); i++){
        bads.push(~b.outs[i]);
        recursiveWriteSmv(f, c, gate(b.outs[i]), reached, tip_mode, structured);
    }
    for (int i = 0; i < flp.size(); i++)
        recursiveWriteSmv(f, c, gate(flp.def(flp[i])), reached, tip_mode, structured);

    fprintf(f, "__prop := ");
    writeSmvWithOp(f, "|", bads);
    fprintf(f, ";\n");

    fprintf(f, "SPEC AG __prop\n");

    fclose(f);
}
