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

#ifndef Minisat_Clausify_h
#define Minisat_Clausify_h

#include "core/SolverTypes.h"
#include "simp/SimpSolver.h"
#include "circ/Circ.h"

namespace Minisat {;

// FIXME: handle constants !!!
//=================================================================================================
// Simple helpers:

template<class S>
static void add1Clause(Lit x, S& solver, vec<Lit>& tmp) { 
    // printf(" [%s%d, %s%d]\n", sign(x)?"~":"", var(x), sign(y)?"~":"", var(y));
    tmp.clear(); tmp.push(x); solver.addClause(tmp); }

template<class S>
static void add2Clause(Lit x, Lit y, S& solver, vec<Lit>& tmp) { 
    // printf(" [%s%d, %s%d]\n", sign(x)?"~":"", var(x), sign(y)?"~":"", var(y));
    tmp.clear(); tmp.push(x); tmp.push(y); solver.addClause(tmp); }

template<class S>
static void add3Clause(Lit x, Lit y, Lit z, S& solver, vec<Lit>& tmp) { 
    tmp.clear(); tmp.push(x); tmp.push(y); tmp.push(z); solver.addClause(tmp); }

template<class S>
static void add4Clause(Lit x, Lit y, Lit z, Lit w, S& solver, vec<Lit>& tmp) {
    tmp.clear(); tmp.push(x); tmp.push(y); tmp.push(z); tmp.push(w); solver.addClause(tmp); }


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

    SSet       top_assumed;

    int nof_ands;
    int nof_xors;
    int nof_muxs;

    // -------------------------------------------------------------------------------------------
    // Clausify:
    //
    void clausifyIter(Gate g)
    {
        vec<Gate> stack; stack.push(g);
        assert(vmap[g] == vmap[g]);
        while (stack.size() > 0){
            g = stack.last(); 
            assert(g != gate_Undef);
            if (g == gate_True){
                // Constant gate:
                //
                if (vmap[g] == var_Undef){
                    vmap[g] = solver.newVar();
                    add1Clause(mkLit(vmap[g]), solver, tmp_lits); }
                stack.pop();

            }else if (type(g) == gtype_Inp){
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

#if 1
                    Sig x, y, z;
                    if (circ.matchMux(g, x, y, z)){
                        stack.push(gate(x));
                        stack.push(gate(y));

                        if (y != ~z)
                            nof_muxs++, stack.push(gate(z));
                        else
                            nof_xors++;
                    }else
#endif
                    // 
                    {
                        nof_ands++;
                        circ.matchAnds(g, tmp_big_and, false);
                        for (int i = 0; i < tmp_big_and.size(); i++)
                            stack.push(gate(tmp_big_and[i]));
                    }

                }else if (vmap[g] == -2){
                    // On way up, generate clauses:
                    //
                    vmap[g] = solver.newVar();
                    Lit lg = mkLit(vmap[g]);

#if 1
                    Sig x, y, z;
                    if (circ.matchMux(g, x, y, z)){
                        assert(vmap[gate(x)] != var_Undef);
                        assert(vmap[gate(y)] != var_Undef);
                        assert(vmap[gate(z)] != var_Undef);

                        assert(vmap[gate(x)] >= 0);
                        assert(vmap[gate(y)] >= 0);
                        assert(vmap[gate(z)] >= 0);

                        Lit lx = mkLit(vmap[gate(x)], sign(x));
                        Lit ly = mkLit(vmap[gate(y)], sign(y));
                        Lit lz = mkLit(vmap[gate(z)], sign(z));


                        // Implication(s) in one direction:
                        add3Clause(~lg, ~lx,  ly, solver, tmp_lits);
                        add3Clause(~lg,  lx,  lz, solver, tmp_lits);

                        // Implication(s) in other direction:
                        add3Clause( lg, ~lx, ~ly, solver, tmp_lits);
                        add3Clause( lg,  lx, ~lz, solver, tmp_lits);
                    }else
#endif
                    {
                        circ.matchAnds(g, tmp_big_and, false);
                    
                        for (int i = 0; i < tmp_big_and.size(); i++)
                            assert(tmp_big_and[i] != sig_True);
                        
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
                    }

                    assert(solver.okay());
                    stack.pop();
                }else
                    stack.pop();
        }
        
    }


 public:
    Clausifyer(Circ& c, S& s) : 
          circ(c)
        , solver(s)
        , nof_ands(0)
        , nof_xors(0)
        , nof_muxs(0)
        {}

    Var  clausify      (Gate g){ circ.adjustMapSize(vmap, var_Undef); clausifyIter(g); return vmap[g]; }
    Lit  clausify      (Sig  x){ return mkLit(clausify(gate(x)), sign(x)); }

    Lit  clausifyAs    (Gate g, Lit a){ return clausifyAs(mkSig(g), a); }
    Lit  clausifyAs    (Sig  x, Lit a){
        // this is a naive implementation;
        // FIXME: a real implementation avoids the creation of an extra literal
        Lit b = clausify(x);
        solver.addClause(~a,b);
        solver.addClause(a,~b);
        return a;
    }

    Var lookup(Gate g){
        vmap.growTo(g, var_Undef);
        return vmap[g];
    }

    Lit lookup(Sig s){
        vmap.growTo(gate(s), var_Undef);
        if (vmap[gate(s)] == var_Undef)
            return lit_Undef;
        else    
            return mkLit(vmap[gate(s)], sign(s));
    }

    lbool modelValue(Gate g){
        Var x = lookup(g);
        return x == var_Undef ? l_Undef
             : solver.modelValue(x);
    }

    lbool modelValue(Sig s){
        return modelValue(gate(s)) ^ sign(s);
    }

    void assume(Sig x){
        vec<Sig> top;
        vec<Sig> disj;
        vec<Lit> lits;

        if (sign(x) || type(x) == gtype_Inp)
            top.push(x);
        else
            circ.matchAnds(gate(x), top, false);

        // if (top.size() > 1)
        //     printf(" >>> Gathered %d top level gates\n", top.size());

        for (int i = 0; i < top.size(); i++)
            if (!top_assumed.has(top[i])){
                top_assumed.insert(top[i]);
                
                if (type(top[i]) == gtype_Inp || !sign(top[i]))
                    add1Clause(clausify(top[i]), solver, tmp_lits);
                else{

                    circ.matchAnds(gate(top[i]), disj, false);
                    lits.clear();
                    for (int j = 0; j < disj.size(); j++)
                        lits.push(clausify(~disj[j]));
                    solver.addClause(lits);
                }
            }
        // fprintf(stderr, " >> (assume) ANDS = %d, XORS = %d, MUXES = %d\n", nof_ands, nof_xors, nof_muxs);
    }
};


class SimpClausifyer
{
 protected:
    Circ&       circ;
    SimpSolver& solver;

    GMap<Var>   vmap;

    vec<Lit>    tmp_lits;
    SSet        tmp_reached;
    vec<Sig>    tmp_sig_stack;
    vec<Sig>    tmp_big_and;
                
    SSet        top_assumed;

    int nof_ands;
    int nof_xors;
    int nof_muxs;

    // -------------------------------------------------------------------------------------------
    // Clausify:
    //
    void clausifyIter(Gate g)
    {
        vec<Gate> stack; stack.push(g);
        assert(vmap[g] == vmap[g]);
        while (stack.size() > 0){
            g = stack.last(); 
            assert(g != gate_Undef);

            zappElimed(g);

            if (g == gate_True){
                // Constant gate:
                //
                if (vmap[g] == var_Undef){
                    vmap[g] = solver.newVar();
                    solver.setFrozen(vmap[g], true);
                    add1Clause(mkLit(vmap[g]), solver, tmp_lits); }
                stack.pop();

            }else if (type(g) == gtype_Inp){
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

#if 0
                    Sig x, y, z;
                    if (circ.matchMux(g, x, y, z)){
                        stack.push(gate(x));
                        stack.push(gate(y));

                        if (y != ~z)
                            nof_muxs++, stack.push(gate(z));
                        else
                            nof_xors++;
                    }else
#endif
                    // 
                    {
                        nof_ands++;
                        circ.matchAnds(g, tmp_big_and, false);
                        for (int i = 0; i < tmp_big_and.size(); i++)
                            stack.push(gate(tmp_big_and[i]));
                    }

                }else if (vmap[g] == -2){
                    // On way up, generate clauses:
                    //
                    vmap[g] = solver.newVar();
                    Lit lg = mkLit(vmap[g]);

#if 0
                    Sig x, y, z;
                    if (circ.matchMux(g, x, y, z)){
                        assert(vmap[gate(x)] != var_Undef);
                        assert(vmap[gate(y)] != var_Undef);
                        assert(vmap[gate(z)] != var_Undef);

                        assert(vmap[gate(x)] >= 0);
                        assert(vmap[gate(y)] >= 0);
                        assert(vmap[gate(z)] >= 0);

                        Lit lx = mkLit(vmap[gate(x)], sign(x));
                        Lit ly = mkLit(vmap[gate(y)], sign(y));
                        Lit lz = mkLit(vmap[gate(z)], sign(z));


                        // Implication(s) in one direction:
                        add3Clause(~lg, ~lx,  ly, solver, tmp_lits);
                        add3Clause(~lg,  lx,  lz, solver, tmp_lits);

                        // Implication(s) in other direction:
                        add3Clause( lg, ~lx, ~ly, solver, tmp_lits);
                        add3Clause( lg,  lx, ~lz, solver, tmp_lits);
                    }else
#endif
                    {
                        circ.matchAnds(g, tmp_big_and, false);
                    
                        for (int i = 0; i < tmp_big_and.size(); i++){
                            assert(tmp_big_and[i] != sig_True);
                            assert(!solver.isEliminated(vmap[gate(tmp_big_and[i])]));
                        }
                        
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
                    }

                    assert(solver.okay());
                    stack.pop();
                }else
                    stack.pop();
        }
        
    }


 public:
    SimpClausifyer(Circ& c, SimpSolver& s) : 
          circ(c)
        , solver(s)
        , nof_ands(0)
        , nof_xors(0)
        , nof_muxs(0)
        {}

    void zappElimed    (Gate g){ 
        //if (vmap[g] != var_Undef && solver.isEliminated(vmap[g])){
        if (vmap[g] >= 0 && solver.isEliminated(vmap[g])){
            // printf("Gate %d:%d zapped .\n", index(g), vmap[g]);
            vmap[g] = var_Undef; 
        }
    }

    Var  clausify      (Gate g){ clausifyIter(g); return vmap[g]; }
    Lit  clausify      (Sig  x){ return mkLit(clausify(gate(x)), sign(x)); }

    Var lookup(Gate g){
        vmap.growTo(g, var_Undef);
        return vmap[g];
    }

    Lit lookup(Sig s){
        vmap.growTo(gate(s), var_Undef);
        if (vmap[gate(s)] == var_Undef)
            return lit_Undef;
        else    
            return mkLit(vmap[gate(s)], sign(s));
    }

    void prepare(){
        circ.adjustMapSize(vmap, var_Undef);
    }

    void assume(Sig x){
        vec<Sig> top;
        vec<Sig> disj;
        vec<Lit> lits;

        if (sign(x) || type(x) == gtype_Inp)
            top.push(x);
        else
            circ.matchAnds(gate(x), top, false);

        // if (top.size() > 1)
        //     printf(" >>> Gathered %d top level gates\n", top.size());

        for (int i = 0; i < top.size(); i++)
            if (!top_assumed.has(top[i])){
                top_assumed.insert(top[i]);
                
                if (type(top[i]) == gtype_Inp || !sign(top[i]))
                    add1Clause(clausify(top[i]), solver, tmp_lits);
                else{

                    circ.matchAnds(gate(top[i]), disj, false);
                    lits.clear();
                    for (int j = 0; j < disj.size(); j++)
                        lits.push(clausify(~disj[j]));
                    solver.addClause(lits);
                }
            }
        // fprintf(stderr, " >> (assume) ANDS = %d, XORS = %d, MUXES = %d\n", nof_ands, nof_xors, nof_muxs);
    }
};

template<>
class Clausifyer<SimpSolver> : public SimpClausifyer 
{
 public:
    Clausifyer(Circ& c, SimpSolver& s) : SimpClausifyer(c, s) {}
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

    void prepare       () {}

    Var lookup(Gate g){
        vmap.growTo(g, var_Undef);
        return vmap[g];
    }

    Lit lookup(Sig s){
        vmap.growTo(gate(s), var_Undef);
        if (vmap[gate(s)] == var_Undef)
            return lit_Undef;
        else    
            return mkLit(vmap[gate(s)], sign(s));
    }

    Lit clausify(Sig  x){ return mkLit(clausify(gate(x)), sign(x)); }
    Var clausify(Gate g){
        //printf("clausify g=%d (gate_Undef=%d)\n", index(g), index(gate_Undef));
        assert(g != gate_Undef);
        // assert(g != gate_True);
        vmap.growTo(g, var_Undef);
        if (vmap[g] == var_Undef){
            vmap[g] = solver.newVar();
            
            if (g == gate_True){
	        add1Clause(mkLit(vmap[g]), solver, tmp_lits); 
	    }else if (type(g) == gtype_And){
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

};

#endif
