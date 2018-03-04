## Gnuplot qalculate plugin #
Let gnuplot to guide the qalculate.  
Note, that you need minimal gnuplot version 5 to use plugins.  

### Installation ###
Run __make__ to build.

### Usage ###
Warning: This is work in progress, some interface details may change in future.  
- After gnuplot start, load functions from plugin, using import command:  
`import qalc_ufunc_new(name, expr)  from "gnuplot-qalc-plugin"`  
`import qalc_ufunc_del(name_or_id) from "gnuplot-qalc-plugin"`  
`import qalc_ufunc_list(x)                       from "gnuplot-qalc-plugin"`  
`import qalc_ufunc_exec(name_or_id, arg) from "gnuplot-qalc-plugin"`

- Load your functions to plugin, using qalculate syntax.  
For example:
`print qalc_ufunc_new('myfunc1', '\x / 4')`  
`print qalc_ufunc_new('myfunc2', '\x^2 / 3')`  
`...`  
when function is created, you will get its number, which may be used for quick access for functions, requiring math function identifier _(see __Function identification__)_.

- To get list of loaded functions:  
`qalc_ufunc_list(0)`   
though, this function really doesn't require parameters, gnuplot doesn't allow to define functions with zero parameters.
You will get list of all previously defined functions, where each entry has following format:  
`<number>. <name>: <expression>`  
- Using entered functions:  
`print qalc_ufunc_exec('myfunc1', 6)` (prints __1.5__)  
`plot 3*x+qalc_ufunc_exec('myfunc2', x)` (to plot __3*x + x^2 / 3__, mixing qalculate with gnuplot's math support)  
To make life easier, lets define gnuplot function:  
`parabola(x) = qalc_ufunc_exec('myfunc2', x)`  
`plot 3*x + parabola(x)`  
_Though all examples above are easier doable with gnuplot, qalculate has much more features, compared to standard C math library._

- To delete functions, which became not needed:  
`print qalc_ufunc_del('myfunc1')` (deleting line function, which is not needed)

#### Function identification ###
Functions, operating with existing entries, accept function identifier as either name (in quotes) or number. Numeric identification may be slightly faster than by symbolic name, but it changes when some functions are removed, as they are not alternative names, but rather position in list.  
__Special number -1 identifies last executed function.__