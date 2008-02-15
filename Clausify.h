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
void add1Clause(Lit x, S& solver, vec<Lit>& tmp) { 
    // printf(" [%s%d, %s%d]\n", sign(x)?"~":"", var(x), sign(y)?"~":"", var(y));
    tmp.clear(); tmp.push(x); solver.addClause(tmp); }

template<class S>
void add2Clause(Lit x, Lit y, S& solver, vec<Lit>& tmp) { 
    // printf(" [%s%d, %s%d]\n", sign(x)?"~":"", var(x), sign(y)?"~":"", var(y));
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
    SSet       tmp_reached;
    vec<Sig>   tmp_sig_stack;
    vec<Sig>   tmp_big_and;

    GMap<int>  n_fanouts;

    SSet       top_assumed;

    // -------------------------------------------------------------------------------------------
    // Collect big conjunctions:
    //
    // NOTE! It uses n_fanouts merely as a guide as to whether a node should be introduced or not.
    //       I.e. it does not matter for correctness if this information is 100% accurate.
    //
    void gatherBigAndIter(vec<Sig>& conj){
        while (tmp_sig_stack.size() > 0){
            Sig x = tmp_sig_stack.last(); tmp_sig_stack.pop();

            if (!tmp_reached.has(x)){
                tmp_reached.insert(x);
                
                if (type(x) == gtype_And && n_fanouts[gate(x)] == 1 && !sign(x)){
                    tmp_sig_stack.push(circ.lchild(x));
                    tmp_sig_stack.push(circ.rchild(x));
                } else
                    conj.push(x);
            }
        }
    }

    void gatherBigAnd(Sig x, vec<Sig>& conj){
        assert(type(x) == gtype_And);
        conj.clear();
        tmp_reached.clear(); 
        tmp_sig_stack.clear();
        tmp_sig_stack.push(circ.lchild(x));
        tmp_sig_stack.push(circ.rchild(x));
        gatherBigAndIter(conj);
    }

    // -------------------------------------------------------------------------------------------
    // Clausify:
    //
    void clausifyIter(Gate g)
    {
        vec<Gate> stack; stack.push(g);

        while (stack.size() > 0){
            g = stack.last(); 

            assert(g != gate_True);
            if (type(g) == gtype_Inp){
                // Input gate:
                //
                if (vmap[g] == var_Undef) 
                    vmap[g] = solver.newVar();
                stack.pop();

            }else if (type(g) == gtype_And)
                // And gate:
                //
                if (vmap[g] == var_Undef){
                    // Mark gate while traversing "downwards":
                    //
                    vmap[g] = -2;
                    gatherBigAnd(mkSig(g), tmp_big_and);
                    for (int i = 0; i < tmp_big_and.size(); i++)
                        stack.push(gate(tmp_big_and[i]));
                }else if (vmap[g] == -2){
                    // On way up, generate clauses:
                    //
                    vmap[g] = solver.newVar();
                    Lit lg = mkLit(vmap[g]);
                    gatherBigAnd(mkSig(g), tmp_big_and);

                    // Implication(s) in one direction:
                    for (int i = 0; i < tmp_big_and.size(); i++){
                        Lit p = mkLit(vmap[gate(tmp_big_and[i])], sign(tmp_big_and[i]));
                        add2Clause(~lg, p, solver, tmp_lits); }

                    // Single implication in other direction:
                    tmp_lits.clear();
                    for (int i = 0; i < tmp_big_and.size(); i++){
                        Lit p = mkLit(vmap[gate(tmp_big_and[i])], sign(tmp_big_and[i]));
                        tmp_lits.push(~p); }
                    tmp_lits.push(lg);
                    solver.addClause(tmp_lits);

                    assert(solver.okay());
                    stack.pop();
                }else
                    stack.pop();
        }
        
    }


 public:
    Clausifyer(Circ& c, S& s) : circ(c), solver(s) {}

    Var  clausify      (Gate g){ clausifyIter(g); return vmap[g]; }
    Lit  clausify      (Sig  x){ return mkLit(clausify(gate(x)), sign(x)); }

    void prepare       () {
        n_fanouts.clear();
        circ.adjustMapSize(n_fanouts, 0);
        circ.adjustMapSize(vmap, var_Undef);

        for (Gate g = circ.firstGate(); g != gate_Undef; g = circ.nextGate(g))
            if (type(g) == gtype_And){
                n_fanouts[gate(circ.lchild(g))]++;
                n_fanouts[gate(circ.rchild(g))]++;
            }
    }

    void assume(Sig x){
        vec<Sig> top;
        vec<Sig> disj;
        vec<Lit> lits;

        tmp_sig_stack.clear();
        tmp_sig_stack.push(x);

        while (tmp_sig_stack.size() > 0){
            x = tmp_sig_stack.last(); tmp_sig_stack.pop();

            if (!top_assumed.has(x)){
                top_assumed.insert(x);
                
                if (type(x) == gtype_Inp)
                    add1Clause(clausify(x), solver, tmp_lits);
                else if (!sign(x)){
                    tmp_sig_stack.push(circ.lchild(x));
                    tmp_sig_stack.push(circ.rchild(x));
                } else
                    top.push(x);
            }
        }
        printf(" >>> Gathered %d top level gates\n", top.size());

        for (int i = 0; i < top.size(); i++){
            assert(sign(top[i]));

            gatherBigAnd(top[i], disj);
            
            lits.clear();
            for (int j = 0; j < disj.size(); j++)
                lits.push(~clausify(disj[j]));
            solver.addClause(lits);
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
