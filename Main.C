/******************************************************************************************[Main.C]
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

#include <cstdio>
#include <cstring>

#include <errno.h>
#include <signal.h>

#include "Options.h"
#include "System.h"
#include "SimpSolver.h"
#include "Circ.h"
#include "Clausify.h"
#include "Aiger.h"


//=================================================================================================


void printStats(Solver& solver)
{
    double cpu_time = cpuTime();
    double mem_used = memUsed();
    reportf("restarts              : %"PRIu64"\n", solver.starts);
    reportf("conflicts             : %-12"PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   /cpu_time);
    reportf("decisions             : %-12"PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions*100 / (float)solver.decisions, solver.decisions   /cpu_time);
    reportf("propagations          : %-12"PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations/cpu_time);
    reportf("conflict literals     : %-12"PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals)*100 / (double)solver.max_literals);
    if (mem_used != 0) reportf("Memory used           : %.2f MB\n", mem_used);
    reportf("CPU time              : %g s\n", cpu_time);
}

SimpSolver* solver;
static void SIGINT_handler(int signum) {
    reportf("\n"); reportf("*** INTERRUPTED ***\n");
    printStats(*solver);
    reportf("\n"); reportf("*** INTERRUPTED ***\n");
    exit(1); }


//=================================================================================================
// Main:


int main(int argc, char** argv)
{
    setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input is in plain or gzipped binary AIGER.\n");
    reportf("Using MiniSat 2.0 beta\n");

#if defined(__linux__)
    fpu_control_t oldcw, newcw;
    _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
    reportf("WARNING: for repeatability, setting FPU to use double precision\n");
#endif

    // Extra options:
    //
    BoolOption   pre            ("MAIN", "pre",    "Completely turn on/off any preprocessing.", true);
    BoolOption   clausify_naive ("MAIN", "clausify-naive", "Use naive clausification", false);
    StringOption aiger          ("MAIN", "aiger",  "If given, stop after preprocessing AIG and write the result to this file.");
    StringOption dimacs         ("MAIN", "dimacs", "If given, stop after producing CNF and write the result to this file.");

    parseOptions(argc, argv, true);

    SimpSolver S;
    double     initial_time = cpuTime();

    if (!pre) S.eliminate(true);

    solver = &S;
    signal(SIGINT,SIGINT_handler);
    signal(SIGHUP,SIGINT_handler);

    vec<Var> input_vars;

    if (argc < 2 || argc > 3)
        printUsageAndExit(argc, argv);
    else {
        reportf("============================[ Problem Statistics ]=============================\n");
        reportf("|                                                                             |\n");

        Circ     c; 
        vec<Sig> inputs;
        vec<Sig> outputs;
        vec<Def> latch_defs;
        readAiger(argv[1], c, inputs, latch_defs, outputs);

        if (latch_defs.size() > 0)
            fprintf(stderr, "ERROR! Sequential circuits not supported!\n"), exit(1);

        if (outputs.size() != 1)
            fprintf(stderr, "ERROR! Exactly 1 output expected, found %d!\n", outputs.size()), exit(1);

        reportf("|  Number of inputs:     %12d                                         |\n", c.nInps());
        reportf("|  Number of outputs:    %12d                                         |\n", outputs.size());
        reportf("|  Number of gates:      %12d                                         |\n", c.nGates());

        double parsed_time = cpuTime();
        reportf("|  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);

        if (aiger != NULL){
            reportf("==============================[ Writing AIGER ]================================\n");
            writeAiger(aiger, c, inputs, latch_defs, outputs);
            exit(0);
        }

        vec<Lit> unit;
        if (clausify_naive){
            NaiveClausifyer<SimpSolver> cl(c, S);
            for (int i = 0; i < outputs.size(); i++){
                unit.clear();
                unit.push(cl.clausify(outputs[i]));
                assert(S.okay());
                assert(S.value(unit.last()) == l_Undef);
                S.addClause(unit);
            }

            for (int i = 0; i < inputs.size(); i++){
                Lit p = cl.clausify(inputs[i]);
                assert(!sign(p));
                input_vars.push(var(p)); }
        }else {
            Clausifyer<SimpSolver> cl(c, S);
            cl.prepare();
#if 0
            for (int i = 0; i < outputs.size(); i++){
                unit.clear();
                unit.push(cl.clausify(outputs[i]));
                assert(S.okay());
                assert(S.value(unit.last()) == l_Undef);
                S.addClause(unit);
            }
#else
            for (int i = 0; i < outputs.size(); i++)
                cl.assume(outputs[i]);
            
#endif

            for (int i = 0; i < inputs.size(); i++){
                Lit p = cl.clausify(inputs[i]);
                assert(!sign(p));
                input_vars.push(var(p)); }
        }

        reportf("|  Number of variables:  %12d                                         |\n", S.nVars());
        reportf("|  Number of clauses:    %12d                                         |\n", S.nClauses());

        double clausify_time = cpuTime();
        reportf("|  Clausify time:        %12.2f s                                       |\n", clausify_time - parsed_time);
    }

    // Freeze input vars:
    //for (int i = 0; i < input_vars.size(); i++)
    //    S.setFrozen(input_vars[i], true);

    if (pre){
        double simplified_time_before = cpuTime();
        S.eliminate(true);
        reportf("|  Simplification time:  %12.2f s                                       |\n", cpuTime() - simplified_time_before);
    }
    reportf("|                                                                             |\n");

    FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;

    if (!S.okay()){
        if (res != NULL) fprintf(res, "0\n"), fclose(res);
        reportf("===============================================================================\n");
        reportf("Solved by simplification\n");
        printStats(S);
        reportf("\n");
        printf("UNSATISFIABLE\n");
        exit(20);
    }

    if (dimacs){
        reportf("==============================[ Writing DIMACS ]===============================\n");
        S.toDimacs(dimacs);
        printStats(S);
        exit(0);
    }else{
        bool ret = S.solve();
        printStats(S);
        reportf("\n");
        printf(ret ? "SATISFIABLE\n" : "UNSATISFIABLE\n");
        if (res != NULL){
            if (ret){
                fprintf(res, "1\n");
                for (int i = 0; i < input_vars.size(); i++){
                    Var inp = input_vars[i];
                    if (S.model[inp] == l_Undef)
                        fprintf(res, "x");
                    else if (S.model[inp] == l_True)
                        fprintf(res, "1");
                    else
                        fprintf(res, "0");
                }
                fprintf(res, "\n");
            }else
                fprintf(res, "0\n");
            fclose(res);
        }
#ifdef NDEBUG
        exit(ret ? 10 : 20);     // (faster than "return", which will invoke the destructor for 'Solver')
#endif
    }
}
