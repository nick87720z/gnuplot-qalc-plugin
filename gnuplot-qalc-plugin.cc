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
#include <vector>

/* pool for functions, entered via gnuplot */
static vector <MathStructure * > ufunc_v;
static vector <char * > ufunc_names;

/* last selected function, for fast access */
static MathStructure * ufunc = NULL;
static int ufunc_id = -1;

/* symbolic variable pool */
static char symch_v[] = "xyzabcdefghijklmnopqrstuvw";
static MathStructure * symstruct [26];
static MathStructure v_xstruct, v_ystruct, v_zstruct;

/* calculation data */
static EvaluationOptions eopt;
static MathStructure result;

static const char help_text[] =
"Available functions:\n"
"- qalc_ufunc_new(name, expr)\n"
"- qalc_ufunc_del(fid)\n"
"- qalc_ufunc_list()\n"
"- qalc_ufunc_exec(fid, arg)\n";

static bool initialized = false;

int ufunc_find (const char * name)
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
            v_xstruct = MathStructure((Variable *)CALCULATOR->v_x);
            v_ystruct = MathStructure((Variable *)CALCULATOR->v_y);
            v_zstruct = MathStructure((Variable *)CALCULATOR->v_z);

            /* Pre-fill symbol math structures */
            char symch [2] = {'0', '\0'};
            char * symp, * symp_end;
            MathStructure ** symsp;

            symp_end = (symp = symch_v) + sizeof(symch_v)/sizeof(char);
            symsp = (MathStructure **)symstruct;
            for (; symp < symp_end; symp++, symsp++)
            {
                symch[0] = *symp;
                *symsp = new MathStructure (symch);
            }

            printf("Qalculate plugin is ready\n\n%s", help_text);

            initialized = true;
        }
    }

    /* By some reason gnuplot_fini() is not invoked by gnuplot */
    struct value qalc_fini (int nargs, struct value *arg, void *p)
    {
        struct value r = { .type=INTGR };
        r.v.int_val = 0;
        if (!initialized) return r;

        printf("Qalculate gnuplot plugin is finishing\n");
        //~ for (auto i = ufunc_v.end(); i == ufunc_v.begin() ; )
        //~ {
            //~ i--;
            //~ ufunc_v.erase(i);
        //~ }
        {
            auto p = symstruct;
            auto endp = symstruct;
            endp += sizeof(symstruct)/sizeof(*symstruct);
            for(; p != endp; p++)
                delete *p;
        }

        ufunc_v.clear();
        ufunc_names.clear();
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
            MathStructure args_v (symstruct[0], NULL);

            //~ mstruct = MathStructure();
            ufunc.calculate(*mstruct, args_v, eopt);
            mstruct->replace(v_xstruct, *(symstruct[0]));
            mstruct->factorize(eopt);
            mstruct->eval(eopt);
            mstruct->simplify(eopt);

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
        } else if (ufid>=ufunc_v.size())
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
            printf ("%zi. %s:\t%s\n", i,
                r.v.data_array[i],
                ufunc_v[i]->print().c_str() );
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
        double x=0.0, y=0.0;
        Number y_num, y_imag;
        struct value r = { .type=INVALID_VALUE };
        int ufid = -1;
        if (!initialized) return r;

        /* parse arguments */
        if (nargs != 2)
            return r;

        /* prepare function */
        switch (arg[0].type) {
            case STRING: {
                ufunc_id = ufunc_find (arg[0].v.string_val);
                if (ufunc_id == -1)
                    return r;
                ufunc = ufunc_v[ufunc_id];
                goto ufarg_check;
            }
            case CMPLX: ufid = (int)arg[0].v.cmplx_val.real; break;
            case INTGR: ufid = arg[0].v.int_val; break;
            default: return r;
        }
        /* skip checks for current selection */
        if (ufid == ufunc_id || ufid == -1) {
            /* but only if selection is valid */
            if (ufunc_id == -1) return r;
            goto ufarg_check;
        }

        /* abort if ufid out of range */
        if (ufid >= ufunc_v.size())
            return r;

        ufunc_id = ufid;
        ufunc = ufunc_v[ufunc_id];

        ufarg_check:;
        switch (arg[1].type) {
            case CMPLX: x = arg[1].v.cmplx_val.real; break;
            case INTGR: x = arg[1].v.int_val; break;
            default: return r;
        }

        /* calculation */
        MathStructure xstruct = MathStructure(x);
        result = MathStructure (*ufunc);
        result.replace((*symstruct)[0], xstruct);
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
