/* gnuplot-qalc-plugin.cc
 *
 * Copyright (c) 2018 Nikita Zlobin (nick87720z@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Begining level functionality:
 * TODO:
 * - support more function arguments
 * - qalculate static stream processor?
 * - (probably) implement separate qalculate operations, missing in libc math
 *
 */

#include <gp_types.h>
#include <libqalculate/Calculator.h>
#include <libqalculate/Function.h>
#include <libqalculate/Variable.h>
#include <string.h>
#include <cstdlib>
#include <cstdint>
#include <vector>

/* pool for functions, entered via gnuplot */
static vector <MathStructure * > ufunc_v;
static vector <char * > ufunc_names;
static vector <int> ufunc_argn;

/* last selected function, for fast access */
static MathStructure * ufunc = NULL;
static int ufunc_id = -1;

/* symbolic variable pool */
static char symch_v[] = "xyzabcdefghijklmnopqrstu";
static MathStructure * symstruct [26];
static MathStructure symvector;
static MathStructure v_xstruct, v_ystruct, v_zstruct;

/* calculation data */
static EvaluationOptions eopt;
static MathStructure result;
static Number y_num, y_imag;
static vector <MathStructure> argstruct_v;

static const char help_text[] =
"Available functions:\n"
"- qalc_ufunc_new(name, expr)\n"
"- qalc_ufunc_del(fid)\n"
"- qalc_ufunc_list()\n"
"- qalc_ufunc_exec(fid, arg)\n";

static bool initialized = false;

static inline bool intmore (signed s, unsigned u) {
    if (s < 0 || u > (unsigned)INT_MAX) return false;
    return (s > (signed)u);
}
static inline bool intless (signed s, unsigned u) {
    if (s < 0 || u > (unsigned)INT_MAX) return true;
    return (s < (signed)u);
}
static inline bool inteq (signed s, unsigned u) {
    if (s < 0 || u > (unsigned)INT_MAX) return false;
    return (s == (signed)u);
}
static inline bool intmoreeq (signed s, unsigned u) {
    if (s < 0 || u > (unsigned)INT_MAX) return false;
    return (s >= (signed)u);
}
static inline bool intlesseq (signed s, unsigned u) {
    if (s < 0 || u > (unsigned)INT_MAX) return true;
    return (s <= (signed)u);
}

static int ufunc_find (const char * name)
{
    if (ufunc_id != -1 && strcmp(ufunc_names [ufunc_id], name) == 0)
        return ufunc_id;

    for (auto p = ufunc_names.begin(), endp = ufunc_names.end(); p != endp; p++)
    {
        if (strcmp(*p, name)==0)
            return p - ufunc_names.begin();
    }
    return -1;
}

extern "C" {
    void * gnuplot_init (struct value (* cb)(int, struct value *, void *))
    {
        if (! CALCULATOR) {
            eopt.parse_options.angle_unit = ANGLE_UNIT_RADIANS;

            new Calculator();
            CALCULATOR->loadGlobalDefinitions();
            CALCULATOR->loadLocalDefinitions();

            /* Pre-fill symbol math structures */
            v_xstruct = MathStructure((Variable *)CALCULATOR->v_x);
            v_ystruct = MathStructure((Variable *)CALCULATOR->v_y);
            v_zstruct = MathStructure((Variable *)CALCULATOR->v_z);

            char symch [2] = {'0', '\0'};
            char * symp, * symp_end;
            MathStructure ** symsp;

            symp_end = (symp = symch_v) + sizeof(symch_v)/sizeof(*symch_v);
            symsp = (MathStructure **)symstruct;
            symvector.setVector(symstruct[0]);
            for (; symp < symp_end; symp++, symsp++)
            {
                symch[0] = *symp;
                *symsp = new MathStructure (symch);
                symvector.addChild(**symsp);
            }

            printf("Qalculate plugin is ready\n\n%s", help_text);

            initialized = true;
        }
        return NULL;
    }

    /* By some reason gnuplot_fini() is not invoked by gnuplot */
    struct value qalc_fini (int nargs, struct value *arg, void *p)
    {
        struct value r = { .type=INTGR };
        r.v.int_val = 0;
        if (!initialized) return r;

        printf("Qalculate gnuplot plugin is finishing\n");
        auto i = symstruct;
        auto endp = symstruct;
        endp += sizeof(symstruct)/sizeof(*symstruct);
        for(; i != endp; i++)
            delete *i;

        ufunc_v.clear();
        ufunc_names.clear();
        argstruct_v.clear();
        initialized = false;
        return r;
    }

    void gnuplot_fini (void *)
    {
        printf( "╭───╮\n"
                "│┃┃┃│ This message is not supposed to appear due to some gnuplot bug. If"
                " you see it, please let me know.\n"
                "│▪▪▪│ By some reason gnuplot did not finalize plugin on termination, due"
                " to what this must be done manually before to quit gnuplot.\n"
                "╰───╯ (in two words, you don't need to run \"qalc_fini\" anymore.)\n" );
        qalc_fini(0, NULL, NULL);
    }

    /* Create new function.
     * Arguments: name, expression
     * Returns: function ID */
    struct value qalc_ufunc_new (int nargs, struct value *arg, void *p)
    {
        struct value r = { .type=INVALID_VALUE };
        int ufid = -1;
        if (!initialized) return r;

        /* check argument types */
        if (nargs != 2)
            return r;
        if (arg[0].type != STRING)
            return r;
        if (arg[1].type != STRING)
            return r;

        /* check, if function with same name exists */
        ufid = ufunc_find (arg[0].v.string_val);
        if (ufid == -1) {
            /* if not, create new one */
            MathStructure * mstruct = new MathStructure();
            UserFunction ufunc ("", "", arg[1].v.string_val);

            ufunc.calculate(*mstruct, symvector, eopt);
            mstruct->replace(v_xstruct, *(symstruct[0]));
            mstruct->replace(v_ystruct, *(symstruct[1]));
            mstruct->replace(v_zstruct, *(symstruct[2]));
            mstruct->eval(eopt);

            /* replace symbols, taking replacement from begining of pool in same order */
            MathStructure unknowns;
            mstruct->findAllUnknowns(unknowns);
            int unknowns_n = unknowns.size();
            char unknowns_str[unknowns_n + 1] = {' ',' ',' ',' ','\0'};
            for(int i = 0; i < unknowns_n; i++)
                unknowns_str[i] = unknowns[i].symbol().c_str()[0];

            char * p = symch_v;
            for(int i = 0; i != unknowns_n; p++)
            {
                char * ch = strchr (unknowns_str, *p);
                if (ch != NULL) {
                    mstruct->replace(unknowns[ch - unknowns_str], *(symstruct[i]));
                    i++;
                }
            }
            mstruct->eval(eopt);

            ufunc_argn.push_back (unknowns_n);
            ufunc_v.push_back (mstruct);
            ufunc_names.push_back (strdup(arg[0].v.string_val));
            ufid = ufunc_v.size() - 1;
        }
        r.v.int_val = ufid, r.type = INTGR;
        return r;
    }

    /* Remove function by its name or ID.
     * Arguments: name or id
     * Returns: 0 on success, NaN on error */
    struct value qalc_ufunc_del (int nargs, struct value *arg, void *p)
    {
        struct value r = { .type=INVALID_VALUE };
        int ufid = -1;
        if (!initialized) return r;

        if (nargs != 1)
            return r;
        switch (arg[0].type) {
            case STRING: ufid = ufunc_find (arg[0].v.string_val); break;
            case CMPLX: ufid = arg[0].v.cmplx_val.real; break;
            case INTGR: ufid = arg[0].v.int_val; break;
            default: return r;
        }
        if (ufid == -1) {
            ufid = ufunc_id;
            if (ufid == -1)
                return r;
        } else if (intmoreeq (ufid, ufunc_v.size()))
            return r;

        /* don't forget to invalidate current selection */
        if (ufid < ufunc_id)
            ufunc_id--;
        else
            ufunc_id = -1, ufunc = NULL;

        {
            auto i = ufunc_v.begin();
            std::advance(i, ufid);
            delete ufunc_v[ufid];
            ufunc_v.erase(i);
        }{
            auto i = ufunc_names.begin();
            std::advance(i, ufid);
            delete ufunc_names[ufid];
            ufunc_names.erase(i);
        }{
            auto i = ufunc_argn.begin();
            std::advance(i, ufid);
            ufunc_argn.erase(i);
        }

        r.v.int_val = 0, r.type = INTGR;
        return r;
    }

    /* Print function list.
     * Arguments: no
     * Returns: array of function names */
    struct value qalc_ufunc_list (int nargs, struct value *arg, void *p)
    {
        struct value r = { .type=DATABLOCK, .v={ .data_array=NULL } };
        size_t i=0, i_max=0;
        if (!initialized) return r;

        if (ufunc_v.empty())
            return r;

        i_max = ufunc_v.size();
        r.v.data_array = (char **) malloc(sizeof(char *) * (i_max+1));

        for (; i < i_max; i++)
        {
            r.v.data_array[i] = (char *)ufunc_names[i];
            printf ("%zi. %s(", i, r.v.data_array[i]);
            if (ufunc_argn[i] > 0)
                printf("%c", symch_v[0]);
            for (int a = 1; a < ufunc_argn[i]; a++)
                printf(",%c", symch_v[a]);
            printf (") = %s\n", ufunc_v[i]->print().c_str() );
        }
        r.v.data_array[i_max] = NULL;
        printf ("selected: %i\n", ufunc_id);

        return r;
    }

    /* Run registered function. Each execution of function set selection to it,
     * so subsequient access time is less (makes much less effect, when identified by ID).
     * Arguments: function name or ID, arguments
     * Returns: function output on success, NaN otherwise */
    struct value qalc_ufunc_exec (int nargs, struct value *arg, void *p)
    {
        /* initial state */
        double x=0.0;
        struct value r = { .type=INVALID_VALUE };
        int ufid = -1;
        if (!initialized) return r;

        /* parse arguments */
        if (nargs < 2)
            return r;

        /* prepare function */
        switch (arg[0].type) {
            case STRING: {
                ufunc_id = ufunc_find (arg[0].v.string_val);
                if (ufunc_id == -1)
                    return r;
                ufid = ufunc_id;
                ufunc = ufunc_v[ufunc_id];
                goto arg2_check;
            }
            case CMPLX: ufid = (int)arg[0].v.cmplx_val.real; break;
            case INTGR: ufid = arg[0].v.int_val; break;
            default: return r;
        }
        /* skip checks for current selection */
        if (ufid == -1) {
            if (ufunc_id == -1)
                return r;
            goto arg2_check;
        } else if (ufid == ufunc_id)
            goto arg2_check;

        /* abort if ufid out of range */
        if (intmoreeq(ufid, ufunc_v.size()))
            return r;

        ufunc_id = ufid;
        ufunc = ufunc_v[ufunc_id];

        /* Two possible variants of math arguments. If first element is array
         * - it is checked. Otherwise, they are treated as variadic
         */
        arg2_check: nargs--;
        struct value * fargs;
        bool use_array = (arg[1].type == ARRAY);
        int f_nargs = use_array ? ((arg[1].v.value_array[0].type == INTGR)? arg[1].v.value_array[0].v.int_val:0) : ufunc_argn[ufid];
        if (nargs < f_nargs)
            return r;
        nargs = f_nargs;


        /* extend space for argument structs, if we get more or them, then earlier */
        int argstruct_v_allocated = argstruct_v.size();
        if (argstruct_v_allocated < nargs)
        {
            for(int i = nargs - argstruct_v_allocated; i != 0; i--)
            {
                argstruct_v.emplace_back(0.0);
            }
        }

        /* initialize argument structs */
        fargs = (use_array ? arg[1].v.value_array : arg) + 1;
        for (int i = 0; i != nargs; i++)
        {
            switch (fargs[i].type) {
                case CMPLX: x = fargs[i].v.cmplx_val.real; break;
                case INTGR: x = fargs[i].v.int_val; break;
                default: return r;
            }
            argstruct_v[i].number().setFloat(x);
        }

        /* calculation */
        result = MathStructure (*ufunc);
        for (int i = 0; i != nargs; i++)
            result.replace(* symstruct[i], argstruct_v[i]);
        result.eval(eopt);
        y_num = result.number();

        /* output sorting */
        if (y_num.isFloatingPoint() || y_num.isComplex() || y_num.isRational() || y_num.isInfinite()) {
            r.v.cmplx_val.real = y_num.floatValue();
            y_imag = y_num.imaginaryPart();
            r.v.cmplx_val.imag = y_imag.floatValue();
            r.type = CMPLX;
            return r;
        }
        return r;
    }
}
