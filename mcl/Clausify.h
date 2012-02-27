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

#include "minisat/core/SolverTypes.h"
#include "minisat/simp/SimpSolver.h"
#include "mcl/Circ.h"
#include "mcl/Matching.h"

namespace Minisat {

// TODO: 
//   -  Remove the 'top_assumed' functionality?
//   -  Remove the gate counters? They are not used at the moment.

//=================================================================================================
// An almost naive clausifyer for Circuits:

template<class S, bool match_bigands = true, bool match_muxes = true, bool extra_clauses = false>
class Clausifyer
{
    const Circ& circ;
    S&          solver;

    GMap<Lit>   vmap;
    GMap<char>  clausify_mark;

    enum { mark_undef = 0, mark_down = 1, mark_done = 2 };

    vec<Lit>    tmp_lits;
    vec<Sig>    tmp_big_and;

    SSet        top_assumed;
    CircMatcher cm;

    int nof_ands;
    int nof_xors;
    int nof_muxs;

    // -------------------------------------------------------------------------------------------
    // Clausify:
    //
    void clausifyIter(Gate g)
    {
        vec<Gate> stack; stack.push(g);

        while (stack.size() > 0){
            g = stack.last(); 
            assert(g != gate_Undef);

            if (clausify_mark[g] == mark_done){
                stack.pop();
                continue; }

            if (g == gate_True){
                // Constant gate:
                //
                assert(clausify_mark[g] == mark_undef);

                if (vmap[g] == lit_Undef)
                    vmap[g] = mkLit(solver.newVar());
                clausify_mark[g] = mark_done;
                solver.addClause(vmap[g]);
                stack.pop();

            }else if (type(g) == gtype_Inp){
                // Input gate:
                //
                assert(clausify_mark[g] == mark_undef);

                if (vmap[g] == lit_Undef)
                    vmap[g] = mkLit(solver.newVar());
                clausify_mark[g] = mark_done;
                stack.pop();

            }else if (type(g) == gtype_And){
                // And gate:
                //
                if (clausify_mark[g] == mark_undef){
                    // Mark gate while traversing "downwards":
                    //
                    clausify_mark[g] = mark_down;

                    Sig x, y, z;
                    if (match_muxes && cm.matchMux(circ, g, x, y, z)){
                        stack.push(gate(x));
                        stack.push(gate(y));
                        
                        if (y != ~z)
                            nof_muxs++, stack.push(gate(z));
                        else
                            nof_xors++;
                    }else if (match_bigands){
                        nof_ands++;
                        cm.matchAnds(circ, g, tmp_big_and, false);
                        for (int i = 0; i < tmp_big_and.size(); i++)
                            stack.push(gate(tmp_big_and[i]));
                    }else{
                        nof_ands++;
                        stack.push(gate(circ.lchild(g)));
                        stack.push(gate(circ.rchild(g)));
                    }

                }else if (clausify_mark[g] == mark_down){
                    // On way up, generate clauses:
                    //
                    clausify_mark[g] = mark_done;

                    if (vmap[g] == lit_Undef) vmap[g] = mkLit(solver.newVar());
                    Lit lg = vmap[g];

                    // Make sure that this gate is never expaded in future big-and matches:
                    cm.pin(circ, g);

                    Sig x, y, z;
                    if (match_muxes && cm.matchMux(circ, g, x, y, z)){
                        assert(vmap[gate(x)] != lit_Undef);
                        assert(vmap[gate(y)] != lit_Undef);
                        assert(vmap[gate(z)] != lit_Undef);

                        Lit lx = vmap[gate(x)] ^ sign(x);
                        Lit ly = vmap[gate(y)] ^ sign(y);
                        Lit lz = vmap[gate(z)] ^ sign(z);

                        // Implication(s) in one direction:
                        solver.addClause(~lg, ~lx,  ly);
                        solver.addClause(~lg,  lx,  lz);

                        // Implication(s) in other direction:
                        solver.addClause( lg, ~lx, ~ly);
                        solver.addClause( lg,  lx, ~lz);

                        // Extra clauses:
                        if (extra_clauses){
                            solver.addClause(~ly, ~lz,  lg);
                            solver.addClause( ly,  lz, ~lg); }
                    }else if (match_bigands){
                        cm.matchAnds(circ, g, tmp_big_and, false);
                    
                        for (int i = 0; i < tmp_big_and.size(); i++)
                            assert(tmp_big_and[i] != sig_True);
                        
                        // Implication(s) in one direction:
                        for (int i = 0; i < tmp_big_and.size(); i++){
                            Lit p = vmap[gate(tmp_big_and[i])] ^ sign(tmp_big_and[i]);
                            solver.addClause(~lg, p); }
                        
                        // Single implication in other direction:
                        tmp_lits.clear();
                        for (int i = 0; i < tmp_big_and.size(); i++){
                            Lit p = vmap[gate(tmp_big_and[i])] ^ sign(tmp_big_and[i]);
                            tmp_lits.push(~p); }
                        tmp_lits.push(lg);
                        solver.addClause(tmp_lits);
                    }else{
                        Sig x  = circ.lchild(g);
                        Sig y  = circ.rchild(g);
                        Lit lx = vmap[gate(x)] ^ sign(x);
                        Lit ly = vmap[gate(y)] ^ sign(y);

                        solver.addClause(~lg, lx);
                        solver.addClause(~lg, ly);
                        solver.addClause(~lx, ~ly, lg);
                    }

                    // assert(solver.okay());
                    stack.pop();
                }else
                    stack.pop();
            }
        }
        
    }


 public:
    Clausifyer(const Circ& c, S& s) : 
          circ(c)
        , solver(s)
        , nof_ands(0)
        , nof_xors(0)
        , nof_muxs(0)
        {}

    Lit  clausify      (Gate g){ 
        vmap         .growTo(circ.lastGate(), lit_Undef);
        clausify_mark.growTo(circ.lastGate(), mark_undef);
        clausifyIter(g); 
        return vmap[g]; }

    Lit  clausify      (Sig  x){ return clausify(gate(x)) ^ sign(x); }

    void clausifyAs    (Sig  x, Lit a){ clausifyAs(gate(x), a ^ sign(x)); }
    void clausifyAs    (Gate g, Lit a){
        vmap         .growTo(circ.lastGate(), lit_Undef);
        clausify_mark.growTo(circ.lastGate(), mark_undef);

        if (clausify_mark[g] == mark_done){
            Lit b = clausify(g);
            solver.addClause(~a,  b);
            solver.addClause( a, ~b);
        }else{
            vmap[g] = a;
            clausifyIter(g);
        }
    }

    Lit lookup(Gate g){
        assert(g != gate_Undef);
        vmap.growTo(g, lit_Undef);
        return vmap[g];
    }

    Lit lookup(Sig s){
        assert(s != sig_Undef);
        vmap.growTo(gate(s), lit_Undef);
        if (vmap[gate(s)] == lit_Undef)
            return lit_Undef;
        else    
            return vmap[gate(s)] ^ sign(s);
    }

    lbool modelValue(Gate g){
        Lit x;
        return g == gate_Undef || (x = lookup(g)) == lit_Undef ? l_Undef
             : solver.modelValue(x);
    }

    lbool modelValue(Sig s){
        Lit x;
        return s == sig_Undef || (x = lookup(s)) == lit_Undef ? l_Undef
             : solver.modelValue(x);
    }

    lbool modelValue(Gate g, GMap<lbool>& model){
        model.growTo(g, l_Undef);
        Lit x = lookup(g);
        if (x == lit_Undef){
            if (g == gate_True)
                return l_True;
            else if (type(g) == gtype_Inp)
                return l_Undef;
            else if (type(g) == gtype_And && model[g] == l_Undef){
                lbool xv = modelValue(circ.lchild(g), model);
                lbool yv = modelValue(circ.rchild(g), model);
                lbool gv = xv && yv;
                model[g] = gv;
            }
            return model[g];
        }else
            return solver.modelValue(x);
    }

    lbool modelValue(Sig x, GMap<lbool>& model){
        lbool tmp = modelValue(gate(x), model);
        return tmp == l_Undef ? l_Undef : tmp ^ sign(x); 
    }


    void assume(Sig x){
        vec<Sig> top;
        vec<Sig> disj;
        vec<Lit> lits;

        if (type(x) == gtype_Const){
            if (x == sig_False)
                solver.addEmptyClause();
            return;
        }else if (sign(x) || type(x) == gtype_Inp)
            top.push(x);
        else
            cm.matchAnds(circ, gate(x), top, false);

        // NOTE: matchAnds can detect an inconsistency returning a single
        // element list containing sig_False. Handle that:
        if (top.size() == 1 && top[0] == sig_False){
            solver.addEmptyClause();
            return;
        }

        for (int i = 0; i < top.size(); i++){
            assert(type(top[i]) != gtype_Const);
            if (!top_assumed.has(top[i])){
                top_assumed.insert(top[i]);
                
                if (type(top[i]) == gtype_Inp || !sign(top[i]))
                    solver.addClause(clausify(top[i]));
                else{
                    cm.matchAnds(circ, gate(top[i]), disj, false);
                    lits.clear();
                    for (int j = 0; j < disj.size(); j++)
                        lits.push(clausify(~disj[j]));
                    solver.addClause(lits);
                }
            }
        }
        // fprintf(stderr, " >> (assume) ANDS = %d, XORS = %d, MUXES = %d\n", nof_ands, nof_xors, nof_muxs);
    }

    void clear(bool dealloc = false)
    {
        vmap.clear(dealloc);
        clausify_mark.clear(dealloc);
        top_assumed.clear(dealloc);
        nof_ands = 0;
        nof_xors = 0;
        nof_muxs = 0;
    }
};


#if 0
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
                    solver.addClause(mkLit(vmap[g])); }
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
                        solver.addClause(~lg, ~lx,  ly);
                        solver.addClause(~lg,  lx,  lz);

                        // Implication(s) in other direction:
                        solver.addClause( lg, ~lx, ~ly);
                        solver.addClause( lg,  lx, ~lz);
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
                            solver.addClause(~lg, p); }
                        
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
        vmap.growTo(circ.lastGate(), var_Undef);
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
                    solver.addClause(clausify(top[i]));
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

#endif

//=================================================================================================
// A naive clausifyer for Circuits:


template<class S>
class NaiveClausifyer
{
    Circ&      circ;
    S&         solver;

    GMap<Var>  vmap;

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
	        solver.addClause(mkLit(vmap[g]));
	    }else if (type(g) == gtype_And){
                Lit zl = mkLit(vmap[g]);
                Lit xl = clausify(circ.lchild(g));
                Lit yl = clausify(circ.rchild(g));

                solver.addClause(~xl, ~yl, zl);
                solver.addClause(~zl,  xl);
                solver.addClause(~zl,  yl);
                assert(solver.okay());
            }
        }

        return vmap[g];
    }
};


//=================================================================================================

};

#endif
