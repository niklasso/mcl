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

#include "SimpSolver.h"
#include "Circ.h"
#include "Clausify.h"
#include "Aiger.h"
#include "System.h"

#include <cstdio>
#include <cstring>

#include <errno.h>
#include <signal.h>

//=================================================================================================


void printStats(Solver& S)
{
    double   cpu_time = cpuTime();
    uint64_t mem_used = memUsed();
    reportf("restarts              : %lld\n", S.starts);
    reportf("conflicts             : %-12lld   (%.0f /sec)\n", S.conflicts   , S.conflicts   /cpu_time);
    reportf("decisions             : %-12lld   (%4.2f %% random) (%.0f /sec)\n", S.decisions, (float)S.rnd_decisions*100 / (float)S.decisions, S.decisions   /cpu_time);
    reportf("propagations          : %-12lld   (%.0f /sec)\n", S.propagations, S.propagations/cpu_time);
    reportf("conflict literals     : %-12lld   (%4.2f %% deleted)\n", S.tot_literals, (S.max_literals - S.tot_literals)*100 / (double)S.max_literals);
    if (mem_used != 0) reportf("Memory used           : %.2f MB\n", mem_used / 1048576.0);
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


void printUsage(char** argv, SimpSolver& S)
{
    reportf("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n\n", argv[0]);
    reportf("OPTIONS:\n\n");
    reportf("  -pre,    -no-pre                     (default: on)\n");
    reportf("  -elim,   -no-elim                    (default: %s)\n", S.use_elim           ? "on" : "off");
    reportf("  -asymm,  -no-asymm                   (default: %s)\n", S.use_asymm          ? "on" : "off");
    reportf("  -rcheck, -no-rcheck                  (default: %s)\n", S.use_rcheck         ? "on" : "off");
    reportf("\n");
    reportf("  -grow          = <integer> [ >= 0  ] (default: %d)\n", S.grow);
    reportf("  -lim           = <integer> [ >= -1 ] (default: %d)\n", S.clause_lim);
    reportf("  -decay         = <double>  [ 0 - 1 ] (default: %g)\n", S.var_decay);
    reportf("  -rnd-freq      = <double>  [ 0 - 1 ] (default: %g)\n", S.random_var_freq);
    reportf("\n");
    reportf("  -dimacs        = <output-file>.\n");
    reportf("  -verbosity     = {0,1,2}             (default: %d)\n", S.verbosity);
    reportf("\n");
}

const char* hasPrefix(const char* str, const char* prefix)
{
    int len = strlen(prefix);
    if (strncmp(str, prefix, len) == 0)
        return str + len;
    else
        return NULL;
}





int main(int argc, char** argv)
{
    //basicCircTest();
    //fullAdderCorrect();
    //multiplierCorrect(4);
    //if (argc == 2)
    //    factorize64(atoll(argv[1]));


    reportf("This is MiniSat 2.0 beta\n");
#if defined(__linux__)
    fpu_control_t oldcw, newcw;
    _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
    reportf("WARNING: for repeatability, setting FPU to use double precision\n");
#endif
    bool           pre    = true;
    const char*    dimacs = NULL;
    SimpSolver     S;
    S.verbosity = 1;

    // Check for help flag:
    for (int i = 0; i < argc; i++)
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0){
            printUsage(argv, S);
            exit(0); }

    // This just grew and grew, and I didn't have time to do sensible argument parsing yet :)
    //
    int         i, j;
    const char* value;
    for (i = j = 0; i < argc; i++){
        if ((value = hasPrefix(argv[i], "-rnd-freq="))){
            double rnd;
            if (sscanf(value, "%lf", &rnd) <= 0 || rnd < 0 || rnd > 1){
                reportf("ERROR! illegal rnd-freq constant %s\n", value);
                exit(0); }
            S.random_var_freq = rnd;

        }else if ((value = hasPrefix(argv[i], "-decay="))){
            double decay;
            if (sscanf(value, "%lf", &decay) <= 0 || decay <= 0 || decay > 1){
                reportf("ERROR! illegal decay constant %s\n", value);
                exit(0); }
            S.var_decay = 1 / decay;

        }else if ((value = hasPrefix(argv[i], "-verbosity="))){
            int verbosity = (int)strtol(value, NULL, 10);
            if (verbosity == 0 && errno == EINVAL){
                reportf("ERROR! illegal verbosity level %s\n", value);
                exit(0); }
            S.verbosity = verbosity;

        // Boolean flags:
        //
        }else if (strcmp(argv[i], "-pre") == 0){
            pre = true;
        }else if (strcmp(argv[i], "-no-pre") == 0){
            pre = false;
        }else if (strcmp(argv[i], "-asymm") == 0){
            S.use_asymm = true;
        }else if (strcmp(argv[i], "-no-asymm") == 0){
            S.use_asymm = false;
        }else if (strcmp(argv[i], "-rcheck") == 0){
            S.use_rcheck = true;
        }else if (strcmp(argv[i], "-no-rcheck") == 0){
            S.use_rcheck = false;
        }else if (strcmp(argv[i], "-elim") == 0){
            S.use_elim = true;
        }else if (strcmp(argv[i], "-no-elim") == 0){
            S.use_elim = false;
        }else if ((value = hasPrefix(argv[i], "-grow="))){
            int grow = (int)strtol(value, NULL, 10);
            if (grow < 0){
                reportf("ERROR! illegal grow constant %s\n", &argv[i][6]);
                exit(0); }
            S.grow = grow;
        }else if ((value = hasPrefix(argv[i], "-lim="))){
            int lim = (int)strtol(value, NULL, 10);
            if (lim < 3){
                reportf("ERROR! illegal clause limit constant %s\n", &argv[i][5]);
                exit(0); }
            S.clause_lim = lim;
        }else if ((value = hasPrefix(argv[i], "-dimacs="))){
            dimacs = value;
        }else if (strncmp(argv[i], "-", 1) == 0){
            reportf("ERROR! unknown flag %s\nUse -help for more information.\n", argv[i]);
            exit(0);
        }else
            argv[j++] = argv[i];
    }
    argc = j;

    double initial_time = cpuTime();

    if (!pre) S.eliminate(true);

    solver = &S;
    signal(SIGINT,SIGINT_handler);
    signal(SIGHUP,SIGINT_handler);

    if (argc != 2)
        printUsage(argv, S), exit(1);
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
        reportf("|  Number of gates:      %12d                                         |\n", c.nGates());

        vec<Lit> unit;
        Clausifyer<SimpSolver> cl(c, S);
        unit.push(cl.clausify(outputs[0]));
        S.addClause(unit);
        assert(S.okay());

        reportf("|  Number of variables:  %12d                                         |\n", S.nVars());
        reportf("|  Number of clauses:    %12d                                         |\n", S.nClauses());
    }

    double parsed_time = cpuTime();
    reportf("|  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);

    S.eliminate(true);
    double simplified_time = cpuTime();
    reportf("|  Simplification time:  %12.2f s                                       |\n", simplified_time - parsed_time);
    reportf("|                                                                             |\n");

    if (!S.okay()){
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
#ifdef NDEBUG
        exit(ret ? 10 : 20);     // (faster than "return", which will invoke the destructor for 'Solver')
#endif
    }
}
