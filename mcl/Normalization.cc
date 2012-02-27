/********************************************************************************[Normalization.cc]
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

#include <stdio.h>

#include "minisat/mtl/Sort.h"
#include "minisat/mtl/Alg.h"
#include "mcl/Normalization.h"

using namespace Minisat;

//=================================================================================================
// Normalization functions for different n-ary gate types:


void Minisat::normalizeXors(vec<Sig>& xs)
{
    // Calculate the parity and make inputs unsigned:
    //
    bool pol = false;
    for (int i = 0; i < xs.size(); i++){
        pol   = pol ^ sign(xs[i]);
        xs[i] = mkSig(gate(xs[i]));
    }

    // Sort and remove duplicates properly:
    //
    int  i, j, k;
    sort(xs);
    for (i = j = 0; i < xs.size(); i = k){
        // Check how many times xs[i] is repeated:
        for (k = i; k < xs.size() && xs[k] == xs[i]; k++)
            ;

        // fprintf(stderr, "i = %d, j = %d, k = %d, k - i = %d\n", i, j, k, k - i);

        // Only if xs[i] occurs an odd number of times it should be kept:
        if ((k - i) % 2 == 1)
            xs[j++] = xs[i];
    }
    xs.shrink(i - j);

    // Remove possible remaining constant (can only be one, and it must occur first):
    if (xs.size() > 0 && xs[0] == sig_True){
        for (i = 1; i < xs.size(); i++)
            xs[i-1] = xs[i];
        xs.shrink(1);
        pol = !pol;
    }

    if (xs.size() > 0)
        // Attach the sign to the last element:
        xs.last() = xs.last() ^ pol;
    else if (pol)
        // If the xor-chain is empty but the output is expected to be negated, add a constant true:
        xs.push(sig_True);
}


void Minisat::normalizeAnds(vec<Sig>& xs)
{
    int  i, j;
    sort(xs);

    // Remove duplicates and detect inconsistencies:
    for (i = j = 1; i < xs.size(); i++)
        if (xs[i] == ~xs[j-1]){
            xs.clear();
            xs.push(sig_False);
            return;
        }else if (xs[i] != xs[j-1])
            xs[j++] = xs[i];

    xs.shrink(i - j);

    // Remove constant true:
    if (xs.size() > 0 && xs[0] == sig_True){
        for (i = 1; i < xs.size(); i++)
            xs[i-1] = xs[i];
        xs.shrink(1);
    }

    // Detect constant false:
    if (xs.size() > 0 && xs[0] == sig_False){
        xs.clear();
        xs.push(sig_False);
    }
}


void Minisat::normalizeOrs(vec<Sig>& xs)
{
    int  i, j;
    sort(xs);

    // Remove duplicates and detect tautologies:
    for (i = j = 1; i < xs.size(); i++)
        if (xs[i] == ~xs[j-1]){
            xs.clear();
            xs.push(sig_True);
            return;
        }else if (xs[i] != xs[j-1])
            xs[j++] = xs[i];

    xs.shrink(i - j);

    // Detect constant true:
    if (xs.size() > 0 && xs[0] == sig_True){
        xs.clear();
        xs.push(sig_True);
    }

    // Remove constant false:
    if (xs.size() > 0 && xs[0] == sig_False){
        for (i = 1; i < xs.size(); i++)
            xs[i-1] = xs[i];
        xs.shrink(1);
    }
}


// Shamelessly stolen from "SolverTypes.h" ...
static inline Sig subsumes(const vec<Sig>& xs, const vec<Sig>& ys)
{
    if (ys.size() < xs.size())
        return sig_Error;

    Sig ret = sig_Undef;
    for (int i = 0; i < xs.size(); i++){
        // search for xs[i] or ~xs[i]
        for (int j = 0; j < ys.size(); j++)
            if (xs[i] == ys[j])
                goto ok;
            else if (ret == sig_Undef && xs[i] == ~ys[j]){
                ret = ys[j];
                goto ok;
            }

        // did not find it
        return sig_Error;
    ok:;
    }

    return ret;
}


static void printSigs(const vec<Sig>& xs)
{
    fprintf(stderr, "{ ");
    for (int i = 0; i < xs.size(); i++)
        fprintf(stderr, "%s%s%d ", sign(xs[i])?"-":"", type(xs[i]) == gtype_Inp ? "$" : "@", index(gate(xs[i])));
    fprintf(stderr, "}");
}

const vec<vec<Sig> > Minisat::empty_context;

static void subsumptionResolutionSaturation(vec<vec<Sig> >& xss, const vec<vec<Sig> >& context = empty_context)
{
    // fprintf(stderr, "STARTING SUBSUMPTION RESULTION:\n");
    // for (int i = 0; i < xss.size(); i++){
    //     fprintf(stderr, " --- ");
    //     printSigs(xss[i]);
    //     fprintf(stderr, "\n");
    // }
    vec<Sig> tmp;

    // INVARIANT: xs[0] .. xs[i] is normalized.
    //
    for (int i = 0; i < xss.size(); i++){
        // fprintf(stderr, "PROCESSING (i = %d, xss.length = %d)\n", i, xss.size());

        // Check contextual forwards subsumption:
        for (int j = 0; j < context.size(); j++){
            Sig sub_result = subsumes(context[j], xss[i]);

            if (sub_result == sig_Undef){
                // fprintf(stderr, "FWD-CTX-SUB (i = %d, j = %d)\n", i, j);
                // fprintf(stderr, " >>> (i) "); printSigs(xss[i]); fprintf(stderr, "\n");
                // fprintf(stderr, " >>> (j) "); printSigs(context[j]); fprintf(stderr, "\n");
                xss.last().moveTo(xss[i]);
                xss.pop();
                i--;
                goto next_i;
            }else if (sub_result != sig_Error){
                // fprintf(stderr, "FWD-CTX-SUB-RES (i = %d, j = %d)\n", i, j);
                // fprintf(stderr, " >>> (i) "); printSigs(xss[i]); fprintf(stderr, "\n");
                // fprintf(stderr, " >>> (j) "); printSigs(context[j]); fprintf(stderr, "\n");
                remove(xss[i], sub_result);
            }
        }

        // Check forwards subsumption:
        for (int j = 0; j < i; j++){
            Sig sub_result = subsumes(xss[j], xss[i]);

            if (sub_result == sig_Undef){
                // fprintf(stderr, "FWD-SUB (i = %d, j = %d)\n", i, j);
                // fprintf(stderr, " >>> (i) "); printSigs(xss[i]); fprintf(stderr, "\n");
                // fprintf(stderr, " >>> (j) "); printSigs(xss[j]); fprintf(stderr, "\n");
                xss.last().moveTo(xss[i]);
                xss.pop();
                i--;
                goto next_i;
            }else if (sub_result != sig_Error){
                // fprintf(stderr, "FWD-SUB-RES (i = %d, j = %d)\n", i, j);
                // fprintf(stderr, " >>> (i) "); printSigs(xss[i]); fprintf(stderr, "\n");
                // fprintf(stderr, " >>> (j) "); printSigs(xss[j]); fprintf(stderr, "\n");
                remove(xss[i], sub_result);
            }
        }

        // Check backwards subsumption:
        for (int j = 0; j < i; j++){
            Sig sub_result = subsumes(xss[i], xss[j]);

            if (sub_result == sig_Undef){
                // fprintf(stderr, "BWD-SUB (i = %d, j = %d)\n", i, j);
                // fprintf(stderr, " >>> (i) "); printSigs(xss[i]); fprintf(stderr, "\n");
                // fprintf(stderr, " >>> (j) "); printSigs(xss[j]); fprintf(stderr, "\n");
                xss[i-1].moveTo(xss[j]);
                xss[i].moveTo(xss[i-1]);
                xss.last().moveTo(xss[i]);
                xss.pop();
                i--;
                j--;
            }else if (sub_result != sig_Error){
                // fprintf(stderr, "BWD-SUB-RES (i = %d, j = %d)\n", i, j);
                // fprintf(stderr, " >>> (i) "); printSigs(xss[i]); fprintf(stderr, "\n");
                // fprintf(stderr, " >>> (j) "); printSigs(xss[j]); fprintf(stderr, "\n");
                xss[j].moveTo(tmp);
                xss[i-1].moveTo(xss[j]);
                xss[i].moveTo(xss[i-1]);
                xss.last().moveTo(xss[i]);
                tmp.moveTo(xss.last());
                i--;
                remove(xss.last(), sub_result);
            }
        }
    next_i:
        // fprintf(stderr, "NEXT (i = %d, xss.length = %d)\n", i, xss.size());
        ;
    }
}


static inline bool implies(const vec<vec<Sig> >& cnf, const vec<Sig>& clause)
{
    vec<vec<Sig> > tmp;

    // Add unit clauses negating the single input clause:
    //
    for (int i = 0; i < clause.size(); i++){
        tmp.push();
        tmp.last().push(~clause[i]);
    }

    // Copy the input cnf:
    //
    for (int i = 0; i < cnf.size(); i++){
        tmp.push();
        cnf[i].copyTo(tmp.last());
    }

    // Normalize (at least as strong as unit propagation):
    //
    subsumptionResolutionSaturation(tmp);
    if (tmp.size() == 0){
        fprintf(stderr, " >>> WEIRD RESULT FROM IMPLIES CHECK:\n");
        fprintf(stderr, " === ");
        printSigs(clause);
        fprintf(stderr, "\n");

        for (int i = 0; i < cnf.size(); i++){
            fprintf(stderr, " --- ");
            printSigs(cnf[i]);
            fprintf(stderr, "\n");
        } }
            
        
    assert(tmp.size() > 0);

    // Check for the case were the result is a single empty clause:
    //
    return tmp.size() == 1 && tmp[0].size() == 0;
}

#if 1

static inline void removeRedundant(vec<vec<Sig> >& xss)
{
    vec<vec<Sig> > tmp;
    for (int i = 0; i < xss.size(); i++){
        tmp.clear();
        for (int j = 0; j < xss.size(); j++)
            if (i != j){
                tmp.push();
                xss[j].copyTo(tmp.last());
            }

        if (implies(tmp, xss[i])){
            xss.last().moveTo(xss[i]);
            xss.pop();
            i--;
        }
    }
}
#endif

void Minisat::normalizeTwoLevel(vec<vec<Sig> >& xss, const vec<vec<Sig> >& context)
{
    // Basic normalization:
    //
    for (int i = 0; i < xss.size(); i++)
        normalizeOrs(xss[i]);

    for (int i = 0; i < xss.size(); i++)
        if (xss[i].size() == 0){
            xss.clear();
            xss.push();
        }else if (xss[i].size() == 1 && xss[i][0] == sig_True){
            xss.last().moveTo(xss[i]);
            xss.pop();
            i--; }

    #if 0
    int before_size = 0;
    vec<vec<Sig> > copy;
    for (int i = 0; i < xss.size(); i++){
        copy.push();
        xss[i].copyTo(copy.last());
        before_size += copy.last().size();
    }
    #endif

    // if (xss.size() > 15)
    //     fprintf(stderr, "cnf-size: %d .... \n", xss.size());
    // Elaborate normalization:
    //
    if (xss.size() < 17)
        subsumptionResolutionSaturation(xss, context);

    bool check = false;
    for (int i = 0; i < xss.size(); i++)
        check |= xss[i].size() == 0;
    assert(!check || xss.size() == 1);

    #if 0

    int after_size = 0;
    for (int i = 0; i < xss.size(); i++)
        after_size += xss[i].size();

    if (after_size < before_size){
        fprintf(stderr, " >>> SUCCESSFUL REDUNDANCY REMOVAL:\n");
        fprintf(stderr, " --- BEFORE:\n");
        for (int i = 0; i < copy.size(); i++){
            fprintf(stderr, " --- ");
            printSigs(copy[i]);
            fprintf(stderr, "\n");
        }
        fprintf(stderr, " --- AFTER:\n");
        for (int i = 0; i < xss.size(); i++){
            fprintf(stderr, " --- ");
            printSigs(xss[i]);
            fprintf(stderr, "\n");
        }
    }
    #endif

    // Remove remaining clauses that are implied by the others:
    //
#if 1

    if (xss.size() > 1 && xss.size() < 17)
        removeRedundant(xss);

#endif
}
