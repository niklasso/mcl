/*************************************************************************************[SatSweep.cc]
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

#include "minisat/mtl/Sort.h"
#include "minisat/utils/System.h"
#include "mcl/SatSweep.h"

using namespace Minisat;


//=================================================================================================
// Implication set:
//

class ImplSet
{
    Circ       hash;
    SMap<Gate> id_map;

    Gate atom(Sig x){
        id_map.growTo(x, gate_Undef);
        if (id_map[x] == gate_Undef)
            id_map[x] = gate(hash.mkInp());
        return id_map[x];
    }

public:

    bool has   (Sig x, Sig y){
        Gate x_a = atom(x);
        Gate y_a = atom(y);

        return hash.tryAnd(mkSig(x_a), mkSig(y_a)) != sig_Undef;
    }

    void insert(Sig x, Sig y){
        Gate x_a = atom(x);
        Gate y_a = atom(y);
        hash.mkAnd(mkSig(x_a), mkSig(y_a));
    }
};


//=================================================================================================
// Basic helpers:
//

int smallestClass(const Eqs& eqs)
{
    int i = -1;
    int i_smallest = 0;

    for (int k = 0; k < eqs.size(); k++){
        int min_size = index(gate(eqs[k][0]));
        for (int j = 1; j < eqs[k].size(); j++){
            int size = index(gate(eqs[k][j]));
            if (size < min_size)
                min_size = size;
        }
        if (i == -1 || min_size < i_smallest){
            i = k;
            i_smallest = min_size;
        }
    }
    assert(i >= 0);
    assert(i < eqs.size());
    return i;
}


int biggestClass(const Eqs& eqs)
{
    int i = -1;
    int i_largest = 0;

    for (int k = 0; k < eqs.size(); k++){
        int max_size = index(gate(eqs[k][0]));
        for (int j = 1; j < eqs[k].size(); j++){
            int size = index(gate(eqs[k][j]));
            if (size > max_size)
                max_size = size;
        }
        if (i == -1 || max_size > i_largest){
            i = k;
            i_largest = max_size;
        }
    }
    assert(i >= 0);
    assert(i < eqs.size());
    return i;
}

// (stolen from Solver.h)
static inline double drand(double& seed) {
    seed *= 1389796;
    int q = (int)(seed / 2147483647);
    seed -= (double)q * 2147483647;
    return seed / 2147483647; }

// (stolen from Solver.h)
static inline int irand(double& seed, int size) {
    return (int)(drand(seed) * size); }

template<class T>
static void randomShuffle(double& seed, vec<T>& xs)
{
    for (int i = 0; i < xs.size(); i++){
        int pick = i + irand(seed, xs.size() - i);

        assert(pick < xs.size());

        T tmp = xs[i];
        xs[i] = xs[pick];
        xs[pick] = tmp;
    }
}

//=================================================================================================
// Invariant representation:
//

class EqsWithUnits
{
    vec<Sig> units;
    Eqs      eqs;

public:
    void addUnit (Sig x) { units.push(x); }
    void addClass(const vec<Sig>& cls){ eqs.push(); cls.copyTo(eqs.last()); }

    void clear() { units.clear(); eqs.clear(); }
    void moveTo(EqsWithUnits& other){ units.moveTo(other.units); eqs.moveTo(other.eqs); }
    void copyTo(EqsWithUnits& other){ units.copyTo(other.units); copy(eqs, other.eqs); }

    int  nUnits()   const { return units.size(); }
    int  nClasses() const { return eqs.size(); }
    void nonTrivs(int& num, float& avg_size) const {
        int tot   = 0;

        num = 0;
        for (int i = 0; i < eqs.size(); i++)
            if (eqs[i].size() > 1){
                num++;
                tot += eqs[i].size();
            }
        avg_size = (float)tot / num;
    }

    template<class SomeSolver>
    bool falsify(const Circ& cin, SomeSolver& s, Clausifyer<SomeSolver>& cl, EqsWithUnits& proven);

    template<class SomeSolver>
    void refine (const Circ& cin, SomeSolver& s, Clausifyer<SomeSolver>& cl, EqsWithUnits& refined);

    void toEqs(Eqs& out){
        out.clear();
        if (units.size() > 0){
            out.push();
            out.last().push(sig_True);
            append(units, out.last());
        }
        append(eqs, out);
    }        
};

struct RevSigLt { bool operator()(Sig x, Sig y) const { return y < x; } };

template<class SomeSolver>
bool EqsWithUnits::falsify(const Circ&, SomeSolver& s, Clausifyer<SomeSolver>& cl, EqsWithUnits& proven)
{
    // Find trivial units:
    //
    int i, j;
    for (i = j = 0; i < units.size(); i++){
        Sig x = units[i];
        Lit p = cl.clausify(x);

        if (s.value(p) == l_True)
            proven.addUnit(x);
        else
            units[j++] = units[i];
    }
    units.shrink(i - j);

    // Find trivial classes:
    //
    for (i = j = 0; i < eqs.size(); i++)
        if (eqs[i].size() == 1)
            proven.addClass(eqs[i]);
        else{
            if (i != j)
                eqs[i].moveTo(eqs[j]);
            j++;
        }
    eqs.shrink(i - j);

    // Prove remaining units:
    //
    // sort(units, RevSigLt());
    // sort(units);
    while (units.size() > 0){
        Sig x = units.last();
        Lit p = cl.clausify(x);
        if (s.solve(~p))
            return true;

        proven.addUnit(x);
        units.pop();
    }

    // Prove non-unit equivalences:
    //
    while (eqs.size() > 0){
        int i = smallestClass(eqs);
        sort(eqs[i]);
        // This class should not contain units:
        //
        assert(eqs[i].size() > 1);
        assert(!find(eqs[i], sig_True));

        for (int j = 0; j < eqs[i].size(); j++){
            Sig  x     = eqs[i][j];
            Sig  y     = eqs[i][(j+1) % eqs[i].size()];
            Lit  x_lit = cl.clausify(x);
            Lit  y_lit = cl.clausify(y);
            bool res   = s.solve(x_lit, ~y_lit);
                
            if (res) return true;
        }
        proven.addClass(eqs[i]);
        
        if (i < eqs.size()-1)
            eqs.last().moveTo(eqs[i]);
        eqs.pop();
    }

    return false;
}


template<class SomeSolver>
void EqsWithUnits::refine(const Circ&, SomeSolver&, Clausifyer<SomeSolver>& cl, EqsWithUnits& refined)
{
    vec<Sig> class_t;
    vec<Sig> class_f;
    for (int i = 0; i < units.size(); i++)
        if (cl.modelValue(units[i]) == l_True)
            refined.addUnit(units[i]);
        else if (cl.modelValue(units[i]) == l_False)
            class_f.push(units[i]);
        else{
            printf("(REFINE) All gates should have values! x = %s%d\n", sign(units[i])?"-":"", index(gate(units[i])));
            assert(false);
        }
    if (class_f.size() > 0)
        refined.addClass(class_f);

    for (int i = 0; i < eqs.size(); i++){
        class_t.clear();
        class_f.clear();
        for (int j = 0; j < eqs[i].size(); j++){
            Sig x = eqs[i][j];

            if (cl.modelValue(x) == l_True)
                class_t.push(x);
            else if (cl.modelValue(x) == l_False)
                class_f.push(x);
            else{
                printf("(REFINE) All gates should have values! x = %s%d\n", sign(x)?"-":"", index(gate(x)));
                assert(false);
            }
        }
        if (class_t.size() > 0) refined.addClass(class_t);
        if (class_f.size() > 0) refined.addClass(class_f);
    }

    // printf("refining (%d, %d) => (%d, %d)\n", nUnits(), nClasses(), refined.nUnits(), refined.nClasses());
}


template<class Solv>
static void printStatistics(int iters, const Solv& s, const EqsWithUnits& cands, const EqsWithUnits& proven)
{
    // int   cands_total = cands.nClasses();
    int   cands_units = cands.nUnits();
    int   cands_non_triv;
    float cands_avg_size;
    cands.nonTrivs(cands_non_triv, cands_avg_size);

    // int   proven_total = proven.nClasses();
    int   proven_units = proven.nUnits();
    int   proven_non_triv;
    float proven_avg_size;
    proven.nonTrivs(proven_non_triv, proven_avg_size);

    printf("| %5d %5.0f %5d | %5d %5.0f %5d | %6d/%6d %9d %4d %5d %6d | %6.1f | (#assigns=%d)\n",
           cands_non_triv, cands_avg_size, cands_units,
           proven_non_triv, proven_avg_size, proven_units,

           s.nFreeVars(), s.nVars(), 
           s.nClauses(),
           
           iters, 

           (int)s.solves, (int)s.conflicts,

           cpuTime(),

           s.nAssigns()
           );
}

int Minisat::satSweep(Circ& cin, Clausifyer<Solver>& cl, Solver& s, const Eqs& eqs_in, Eqs& eqs_out, int verbosity)
{
    if (verbosity >= 1){
        printf("=================================[ SAT Sweeping ]=============================================\n");
        printf("|     CANDIDATES    |      PROVEN       |       SOLVER                              |  TIME  |\n");
        printf("|  NON   AVG.       |  NON   AVG.       |                                           |        |\n");
        printf("|  TRIV  SIZE UNITS |  TRIV  SIZE UNITS |          VARS   CLAUSES ITER SOLVS CONFLS |        |\n");
        printf("==============================================================================================\n"); }

    // Make sure that all gates refered to by some signal in 'eqs_in'
    // are given a variable in the Clausifyer. This could be done
    // nicer I suppose:
    for (int i = 0; i < eqs_in.size(); i++)
        for (int j = 0; j < eqs_in[i].size(); j++)
            cin.bumpFanout(gate(eqs_in[i][j]));

    // Clausify all gates referred to in some equivalence:
    //
    for (int i = 0; i < eqs_in.size(); i++)
        for (int j = 0; j < eqs_in[i].size(); j++)
            cl.clausify(eqs_in[i][j]);


    EqsWithUnits proven;
    EqsWithUnits curr;

    for (int i = 0; i < eqs_in.size(); i++)
        if (find(eqs_in[i], sig_True)){
            // The units class:
            for (int j = 0; j < eqs_in[i].size(); j++)
                if (eqs_in[i][j] != sig_True)
                    curr.addUnit(eqs_in[i][j]);
        }else
            // Other classes class:
            curr.addClass(eqs_in[i]);

    if (verbosity >= 1) printStatistics(-1, s, curr, proven);

    // Iterate prove/refinement loop:
    //
    // EqsWithUnits next;
    int refines = 0;
    for (;;)
        if (curr.falsify(cin, s, cl, proven)){
            refines++;
            EqsWithUnits apa;
            curr.refine(cin, s, cl, apa);
            apa.moveTo(curr);
            if (verbosity >= 1) printStatistics(refines, s, curr, proven);
        }else{
            if (verbosity >= 1) printStatistics(refines, s, curr, proven);
            break;
        }

    proven.toEqs(eqs_out);
    return refines;
}


int Minisat::satSweep(Circ& cin, Clausifyer<SimpSolver>& cl, SimpSolver& s, const Eqs& eqs_in, Eqs& eqs_out, int verbosity)
{
    if (verbosity >= 1){
        printf("=================================[ SAT Sweeping ]=============================================\n");
        printf("|     CANDIDATES    |      PROVEN       |       SOLVER                              |  TIME  |\n");
        printf("|  NON   AVG.       |  NON   AVG.       |                                           |        |\n");
        printf("|  TRIV  SIZE UNITS |  TRIV  SIZE UNITS |          VARS   CLAUSES ITER SOLVS CONFLS |        |\n");
        printf("==============================================================================================\n"); }

    // Make sure that all gates refered to by some signal in 'eqs_in'
    // are given a variable in the Clausifyer. This could be done
    // nicer I suppose:
    for (int i = 0; i < eqs_in.size(); i++)
        for (int j = 0; j < eqs_in[i].size(); j++)
            cin.bumpFanout(gate(eqs_in[i][j]));

    // Clausify all gates referred to in some equivalence:
    //
    GSet frozen;
    for (int i = 0; i < eqs_in.size(); i++)
        for (int j = 0; j < eqs_in[i].size(); j++){
            Gate g = gate(eqs_in[i][j]);
            Lit  p = cl.clausify(g);

            s.setFrozen(var(p), true);
            frozen.insert(g);
        }


    EqsWithUnits proven;
    EqsWithUnits curr;

    for (int i = 0; i < eqs_in.size(); i++)
        if (find(eqs_in[i], sig_True)){
            // The units class:
            for (int j = 0; j < eqs_in[i].size(); j++)
                if (eqs_in[i][j] != sig_True)
                    curr.addUnit(eqs_in[i][j]);
        }else
            // Other classes class:
            curr.addClass(eqs_in[i]);

    if (verbosity >= 1) printStatistics(-1, s, curr, proven);
    s.eliminate();
    if (verbosity >= 1) printStatistics(-1, s, curr, proven);

    // Iterate prove/refinement loop:
    //
    // EqsWithUnits next;
    int refines = 0;
    int assigns = s.nAssigns();
    for (;;)
        if (curr.falsify(cin, s, cl, proven)){
            refines++;
            EqsWithUnits apa;
            curr.refine(cin, s, cl, apa);
            apa.moveTo(curr);
            if (assigns < s.nAssigns()){
                s.eliminate();
                assigns = s.nAssigns();
            }
            if (verbosity >= 1) printStatistics(refines, s, curr, proven);
        }else{
            if (verbosity >= 1) printStatistics(refines, s, curr, proven);
            break;
        }

    proven.toEqs(eqs_out);

    // Unfreeze variables:
    //
    for (int i = 0; i < frozen.size(); i++){
        Gate g = frozen[i];
        Lit  p = cl.clausify(g);
        s.setFrozen(var(p), false);
    }

    return refines;
}


// NOTE: this is not useful in the presence of assumptions/constrains.
void Minisat::makeUnitClass(const Circ& cin, Eqs& unit)
{
    unit.clear();
    unit.push();

    GMap<bool> val; val.growTo(cin.lastGate(), 0);

    val[gate_True] = 1;
    for (GateIt git = cin.begin(); git != cin.end(); ++git)
        if (type(*git) == gtype_And){
            Sig x     = cin.lchild(*git);
            Sig y     = cin.rchild(*git);
            val[*git] = (val[gate(x)] ^ sign(x)) && (val[gate(y)] ^ sign(y));
        }

    vec<Sig>& cls = unit.last();
    cls.push(sig_True);
    for (GateIt git = cin.begin(); git != cin.end(); ++git)
        cls.push(mkSig(*git, !val[*git]));
}

