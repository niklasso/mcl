/**************************************************************************************[Clausify.h]
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

#ifndef Clausify_h
#define Clausify_h

#include "Circ.h"
#include "SolverTypes.h"

//=================================================================================================
// Simple helpers:

template<class S>
void add2Clause(Lit x, Lit y, S& solver, vec<Lit>& tmp) { 
    tmp.clear(); tmp.push(x); tmp.push(y); solver.addClause(tmp); }

template<class S>
void add3Clause(Lit x, Lit y, Lit z, S& solver, vec<Lit>& tmp) { 
    tmp.clear(); tmp.push(x); tmp.push(y); tmp.push(z); solver.addClause(tmp); }


//=================================================================================================
// An almost naive clausifyer for Circuits:

template<class S>
class Clausifyer
{
    Circ&      circ;
    S&         solver;

    GMap<Var>  vmap;

    vec<Lit>   tmp_lits;

    GMap<char> seen;
    vec<Gate>  marked;

    GMap<int>  n_fanouts;

    // -------------------------------------------------------------------------------------------
    // Collect big conjunctions:
    //
    // NOTE! It uses n_fanouts merely as a guide as to whether a node should be introduced or not.
    //       I.e. it does not matter for correctness if this information is 100% accurate.
    //
    void gatherBigAndHelper(Sig x, vec<Sig>& conj){
        Gate g = gate(x);

        if (seen[g] == 0){
            seen[g] = 1;
            marked.push(g);
            
            if (type(g) == gtype_And && n_fanouts[g] == 1 && !sign(x)){
                gatherBigAndHelper(circ.lchild(g), conj);
                gatherBigAndHelper(circ.rchild(g), conj);
            } else
                conj.push(x);
        }
    }

    void gatherBigAnd(Sig x, vec<Sig>& conj){
        assert(type(x) == gtype_And);
        marked.clear();
        conj.clear();
        gatherBigAndHelper(circ.lchild(x), conj);
        gatherBigAndHelper(circ.rchild(x), conj);
        for (int i = 0; i < marked.size(); i++)
            seen[marked[i]] = 0;
    }

    // -------------------------------------------------------------------------------------------
    // Clausify:
    //
    Lit clausifyHelper(Sig  x){ return mkLit(clausifyHelper(gate(x)), sign(x)); }
    Var clausifyHelper(Gate g){
        assert(g != gate_True);
        if (vmap[g] == var_Undef){
            vmap[g] = solver.newVar();
            
            if (type(g) == gtype_And){
                vec<Sig> big_and;
                gatherBigAnd(mkSig(g), big_and);
                Lit lg = mkLit(vmap[g]);
                vec<Lit> lits;
                for (int i = 0; i < big_and.size(); i++){
                    assert(gate(big_and[i]) != g);
                    Lit p = clausifyHelper(big_and[i]);
                    lits.push(~p);
                    add2Clause(~lg, p, solver, tmp_lits);
                }
                lits.push(lg);
                solver.addClause(lits);
                assert(solver.okay());
            }
        }

        return vmap[g];
    }

 public:
    Clausifyer(Circ& c, S& s) : circ(c), solver(s) {}

    Var  clausify      (Gate g){ return clausifyHelper(g); }
    Lit  clausify      (Sig  x){ return mkLit(clausify(gate(x)), sign(x)); }
    void addConstraints()      { circ.addConstraints(solver, vmap); }

    void prepare       () {
        n_fanouts.clear();
        n_fanouts.growTo(circ.maxGate(), 0);
        vmap     .growTo(circ.maxGate(), var_Undef);
        seen     .growTo(circ.maxGate(), 0);

        for (Gate g = circ.firstGate(); g != gate_Undef; g = circ.nextGate(g))
            if (type(g) == gtype_And){
                n_fanouts[gate(circ.lchild(g))]++;
                n_fanouts[gate(circ.rchild(g))]++;
            }
    }
};


//=================================================================================================
// A naive clausifyer for Circuits:


template<class S>
class NaiveClausifyer
{
    Circ&      circ;
    S&         solver;

    GMap<Var>  vmap;
    vec<Lit>   tmp_lits;

 public:
    NaiveClausifyer(Circ& c, S& s) : circ(c), solver(s) {}

    Lit clausify(Sig  x){ return mkLit(clausify(gate(x)), sign(x)); }
    Var clausify(Gate g){
        vmap.growTo(g, var_Undef);
        if (vmap[g] == var_Undef){
            vmap[g] = solver.newVar();
            
            if (type(g) == gtype_And){
                Lit zl = mkLit(vmap[g]);
                Lit xl = clausify(circ.lchild(g));
                Lit yl = clausify(circ.rchild(g));

                add3Clause(~xl, ~yl, zl, solver, tmp_lits);
                add2Clause(~zl,  xl,     solver, tmp_lits);
                add2Clause(~zl,  yl,     solver, tmp_lits);
                assert(solver.okay());
            }
        }

        return vmap[g];
    }
};


//=================================================================================================
#endif
