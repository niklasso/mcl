/*************************************************************************************[Hardware.cc]
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

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "minisat/simp/SimpSolver.h"
#include "mcl/Hardware.h"
#include "mcl/Clausify.h"
#include "mcl/CircPrelude.h"

using namespace Minisat;

//=================================================================================================
// Miscellaneous functions for generating hardware such as integer arithmetics, sorting etc.


//-------------------------------------------------------------------------------------------------
// A simple full-adder. Three bits input, two bits output:

void Minisat::fullAdder(Circ& c, Sig x, Sig y, Sig z, Sig& sum, Sig& carry)
{
    Sig w = c.mkXorEven(x, y);
    sum   = c.mkXorEven(w, z);
    carry = c.mkOr(c.mkAnd(x, y), c.mkAnd(z, w));
}


//-------------------------------------------------------------------------------------------------
// Adder network:

static inline void pop2(vec<Sig>& xs, Sig& x, Sig& y){ x = xs.last(); xs.pop(); y = xs.last(); xs.pop(); }
static inline void pop3(vec<Sig>& xs, Sig& x, Sig& y, Sig& z){ pop2(xs, x, y); z = xs.last(); xs.pop(); }

void Minisat::dadaAdder(Circ& c, vec<vec<Sig> >& columns, vec<Sig>& result)
{
    Sig x,y,z, sum,carry;

    for (int i = 0; i < columns.size(); i++)
        while (columns[i].size() > 1){
            if (columns[i].size() == 2){
                pop2(columns[i], x, y);
                sum   = c.mkXorEven(x, y);
                carry = c.mkAnd(x, y);
            }else{
                pop3(columns[i], x, y, z);
                fullAdder(c, x, y, z, sum, carry);
            }
            columns.growTo(i+2);
            columns[i+1].push(carry);
            columns[i]  .push(sum);
        }

    result.clear();
    for (int i = 0; i < columns.size(); i++){
        assert(columns[i].size() == 1);
        result.push(columns[i].last());
    }
}


//-------------------------------------------------------------------------------------------------
// Multiplier:

void Minisat::multiplier(Circ& c, vec<Sig>& xs, vec<Sig>& ys, vec<Sig>& result)
{
    vec<vec<Sig> > columns;
    for (int i = 0; i < xs.size(); i++)
        for (int j = 0; j < ys.size(); j++){
            columns.growTo(i+j+1);
            columns[i+j].push(c.mkAnd(xs[i], ys[j]));
        }

    dadaAdder(c, columns, result);
}


//-------------------------------------------------------------------------------------------------
// Squarer:

static void squarer(Circ& c, vec<Sig>& xs, vec<vec<Sig> >& columns)
{
    columns.clear();
    for (int i = 0; i < xs.size(); i++)
        for (int j = 0; j < i; j++){
            columns.growTo(i+j+2);
            columns[i+j+1].push(c.mkAnd(xs[i], xs[j]));
        }
            
    for (int i = 0; i < xs.size(); i++)
        columns[i].push(xs[i]);
}

void squarer(Circ& c, vec<Sig>& xs, vec<Sig>& result)
{
    vec<vec<Sig> > columns;
    squarer  (c, xs, columns);
    dadaAdder(c, columns, result);
}


//=================================================================================================
// Debug etc:


void Minisat::fullAdderCorrect(void)
{
    Circ c;

    Sig x = c.mkInp();
    Sig y = c.mkInp();
    Sig z = c.mkInp();

    Sig sum, carry;
    fullAdder(c, x, y, z, sum, carry);

    SimpSolver             s;
    Clausifyer<SimpSolver> Cl(c, s);
    Cl.clausify(sum);
    Cl.clausify(carry);
    printf("Full adder number of gates = %d, number of clauses = %d\n", c.nGates(), s.nClauses());

    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            for (int k = 0; k < 2; k++){
                GMap<lbool> values;
                values.growTo(gate(z), l_Undef);
                values[gate(x)] = lbool((bool)i);
                values[gate(y)] = lbool((bool)j);
                values[gate(z)] = lbool((bool)k);
                evaluate(c, sum,   values);
                evaluate(c, carry, values);

#ifndef NDEBUG
                bool sum_   = (values[gate(sum)]   ^ sign(sum)) == l_True;
                bool carry_ = (values[gate(carry)] ^ sign(carry)) == l_True;
#endif                
                //printf("x = %d, y = %d, z = %d, sum = %d, carry = %d\n", 
                //       toInt(values[gate(x)]), toInt(values[gate(y)]), toInt(values[gate(z)]), 
                //       toInt(values[gate(sum)] ^ sign(sum)), toInt(values[gate(carry)] ^ sign(sum)));
                assert(i + j + k == (int)sum_ + (int)carry_*2);
            }


    printf("Full adder correct.\n");
}


static inline void setValue(vec<Sig>& xs, int n, GMap<lbool>& values)
{
    for (int i = 0; i < xs.size(); i++)
        values[gate(xs[i])] = lbool((bool)(n & (1 << i)));
}

static inline int readValue(vec<Sig>& xs, GMap<lbool>& values)
{
    int n = 0;
    for (int i = 0; i < xs.size(); i++){
        assert(values[gate(xs[i])] != l_Undef);
        n += (int)((values[gate(xs[i])] ^ sign(xs[i])) == l_True) << i;
    }
    return n;
}



void Minisat::multiplierCorrect(int size)
{
    Circ c;

    vec<Sig> xs; for (int i = 0; i < size; i++) xs.push(c.mkInp());
    vec<Sig> ys; for (int i = 0; i < size; i++) ys.push(c.mkInp());

    vec<Sig> result;

    multiplier(c, xs, ys, result);
#ifndef NDEBUG
    int size_before = c.nGates();
#endif
    multiplier(c, xs, ys, result);
    assert(c.nGates() == size_before);

    for (int i = 0; i < (1 << size); i++)
        for (int j = 0; j < (1 << size); j++){
            GMap<lbool> values; values.growTo(gate(ys.last()), l_Undef);
            setValue(xs, i, values);
            setValue(ys, j, values);
            for (int k = 0; k < result.size(); k++)
                evaluate(c, result[k], values);
#ifndef NDEBUG
            int n = readValue(result, values);
#endif
            //printf("%d * %d = %d\n", i, j, n);
            assert(i * j == n);
        }

    SimpSolver             s;
    Clausifyer<SimpSolver> Cl(c, s);
    for (int i = 0; i < result.size(); i++)
        Cl.clausify(result[i]);

    printf("Multiplier of size %d number of gates = %d, number of clauses = %d, output bits = %d\n", size, c.nGates(), s.nClauses(), result.size());
    printf("Multiplier of size %d correct.\n", size);
}

/*
static void readBinary(const char* number, vec<bool>& output_bits){
    for (int i = strlen(number)-1; i >= 0; i--)
        if (number[i] == '0')
            output_bits.push(false);
        else if (number[i] == '1')
            output_bits.push(true);
        else{
            fprintf(stderr, "Invalid binary number: %s\n", number);
        }
}


void factorize(const char* number)
{
    
}

*/

static void binarizeNumber(uint64_t number, vec<bool>& output_bits)
{
    while (number > 0){
        output_bits.push(bool(number & 1));
        number >>= 1;
    }
}

static uint64_t unbinarizeSolution(const vec<Sig>& xs, Clausifyer<SimpSolver>& cl, SimpSolver& s)
{
    uint64_t result = 0;

    for (int i = 0; i < xs.size(); i++){
        Lit p = cl.clausify(xs[i]);
        assert(!sign(p));
        assert(s.modelValue(p) != l_Undef);

        if (s.modelValue(p) == l_True)
            result += (1ULL << i);
    }
    return result;
}


static uint64_t nBits(uint64_t number)
{
    uint64_t result = 0;
    for (; number > 0; result++)
        number >>= 1;
    return result;
}


void Minisat::factorize64(uint64_t number)
{
    Circ      c;
    vec<bool> binary_number; binarizeNumber(number, binary_number);
    uint64_t  iroot     = (uint64_t)floor(sqrt((double)number));
    int       xs_length = nBits(iroot);
    int       ys_length = nBits((uint64_t)ceil((double)number / iroot));

    //if (((1ULL << xs_length)-1)*((1ULL << ys_length)-1) < number){
    //    printf("NO FACTORS (trivially)\n");
    //    return; }

    vec<Sig>  xs; for (int i = 0; i < xs_length; i++) xs.push(c.mkInp());
    vec<Sig>  ys; for (int i = 0; i < ys_length; i++) ys.push(c.mkInp());
    vec<Sig>  result;
    multiplier(c, xs, ys, result);

    SimpSolver s;
    Clausifyer<SimpSolver> cl(c, s);

    for (int i = 0; i < result.size(); i++){
        bool value = i < binary_number.size() ? binary_number[i] : false;
        Lit  p     = cl.clausify(result[i]);
        vec<Lit> tmp; tmp.push(p ^ !value); s.addClause(tmp);
    }

    printf("factorizing: %"PRIu64" - binary: ", number);
    for (int i = binary_number.size()-1; i >= 0; i--)
        if (binary_number[i])
            printf("1");
        else
            printf("0");
    printf("\n");
    printf("largest square smaller than target = %"PRIu64"\n", iroot);
    printf("xs bits = %d\n", xs_length);
    printf("ys bits = %d\n", ys_length);

    s.verbosity = 1;
    s.toDimacs("fisk.cnf");
    if (s.solve()){
        printf("SOLUTION %"PRIu64" = %"PRIu64" * %"PRIu64"\n", number, unbinarizeSolution(xs, cl, s), unbinarizeSolution(ys, cl, s));
    }else
        printf("NO FACTORS\n");
}


void Minisat::factorize64squarer(uint64_t number)
{
    Circ      c;
    vec<bool> binary_number; binarizeNumber(number, binary_number);
    uint64_t  iroot     = (uint64_t)floor(sqrt((double)number));
    int       xs_length = nBits(iroot);
    int       ys_length = nBits((uint64_t)ceil((double)number / iroot));

    //if (((1ULL << xs_length)-1)*((1ULL << ys_length)-1) < number){
    //    printf("NO FACTORS (trivially)\n");
    //    return; }

    vec<Sig>  xs; for (int i = 0; i < xs_length; i++) xs.push(c.mkInp());
    vec<Sig>  ys; for (int i = 0; i < ys_length; i++) ys.push(c.mkInp());
    vec<Sig>  result;
    multiplier(c, xs, ys, result);

    SimpSolver s;
    Clausifyer<SimpSolver> cl(c, s);

    for (int i = 0; i < result.size(); i++){
        bool value = i < binary_number.size() ? binary_number[i] : false;
        Lit  p     = cl.clausify(result[i]);
        vec<Lit> tmp; tmp.push(p ^ !value); s.addClause(tmp);
    }

    printf("factorizing: %"PRIu64" - binary: ", number);
    for (int i = binary_number.size()-1; i >= 0; i--)
        if (binary_number[i])
            printf("1");
        else
            printf("0");
    printf("\n");
    printf("largest square smaller than target = %"PRIu64"\n", iroot);
    printf("xs bits = %d\n", xs_length);
    printf("ys bits = %d\n", ys_length);

    s.verbosity = 1;
    s.toDimacs("fisk.cnf");
    if (s.solve()){
        printf("SOLUTION %"PRIu64" = %"PRIu64" * %"PRIu64"\n", number, unbinarizeSolution(xs, cl, s), unbinarizeSolution(ys, cl, s));
    }else
        printf("NO FACTORS\n");
}


//=================================================================================================
