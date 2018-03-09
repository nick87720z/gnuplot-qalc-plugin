## Gnuplot qalculate plugin #
Let gnuplot to guide the qalculate.  
Note, that you need minimal gnuplot version 5 to use plugins.  

### Installation ###
You need gnuplot sources in order to compile this plugin, unless gnuplot moves necessary headers to public API and installs to proper location.
In order to build plugin, run:  
`$ make GNUPLOT_SOURCES_DIR=<path>`  

### Quick start ###
- After gnuplot start, functions should be loaded:  
`import qalc_ufunc_new(name, expr)  from "gnuplot-qalc-plugin"`  
`import qalc_ufunc_del(name_or_id) from "gnuplot-qalc-plugin"`  
`import qalc_ufunc_list(x)          from "gnuplot-qalc-plugin"`  
`import qalc_ufunc_exec(name_or_id, arg) from "gnuplot-qalc-plugin"`

- Load your functions to plugin, using qalculate syntax (see __Argument placeholder syntax__).
`print qalc_ufunc_new('line', 'x / 4')`  
`print qalc_ufunc_new('example2', 'x^2 / 3y')`  
`...`  
If some functions have multiple parameters, as in __example2__, qalc_ufunc_exec must be loaded with same or higher number of arguments. Same function may be imported multiple times under different names:  
`import qalc_ufunc_exec_2a(name_or_id, x, y) from "gnuplot-qalc-plugin"`  
when function is created, you will get its position in list, which may be used for identification _(see __Function identification__)_.  

  - To get list of loaded functions with their numbers:  
`qalc_ufunc_list(0)`   
though, this function really doesn't require parameters, gnuplot doesn't allow to define functions with zero parameters.
You will get list of all previously defined functions, where each entry has following format:  
`<number>. <name>: <expression>`  

- Using entered functions:  
`print qalc_ufunc_exec('myfunc1', 6)` (prints __1.5__)  
`plot 3*x+qalc_ufunc_exec('myfunc2', x, 4.3)` (to plot __3*x + x^2 / 3__, mixing qalculate with gnuplot's math support)  
To make life easier, lets define gnuplot function:  
`parabola(x, y) = qalc_ufunc_exec('myfunc2', x, y)`  
`plot 3*x + parabola(x, 2.9)`  

- To delete functions, which became not needed:  
`print qalc_ufunc_del('myfunc1')` (deleting line function, which is not needed)

### Function identification ###
Functions, operating with existing entries, accept function identifier as either name (in quotes) or number. Numeric identification may be slightly faster than by symbolic name, but it changes when some functions are removed, as they are not alternative names, but rather position in list.  
__Special number -1 identifies last executed function.__

### Argument placeholder syntax ###
Arguments placeholders are named begining from __x,y,z__ and continuing with range __a__..__u__. First three are builtin unknowns, reserved for use in equations, and are usable as is. Other variants must be double-quoted. Using of wirds is possible, but not recommended.

For example:  
`import qalc_ufunc_exec(name_or_id, arg) from "gnuplot-qalc-plugin"`  
`print qalc_ufunc_new('line', 'x / 4')`  
`print qalc_ufunc_new('parabola', 'x^2 + 3y + z')`  
`import qalc_ufunc_exec_3a(name_or_id, x, y, z) from "gnuplot-qalc-plugin:qalc_ufunc_exec"`  
`line_f(x) = qalc_ufunc_exec('line', x)`  
`parabola_f(x) = qalc_ufunc_exec_3a('parabola, x, y, z)'`
