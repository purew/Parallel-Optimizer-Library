Introduction
============

Parallel Optimizer Library was created when I found myself needing 
a way of finding solutions to a multi-dimensional problem where 
exhaustive search is impossible due to the large search-space. 
Essentially, the library tries to quicken the process of finding X 
that maximizes f(X) by implementing multithreaded stocastic 
optimization methods such as particle swarm optimization.

It is implemented using C++ and pthreads and has no additional 
dependencies. My plans are to also implement MPI which would make 
this library far more useful for computer clusters.

It is evolving and the API should not be considered final in any 
way yet. However, I have been using this library for my own 
application since early 2012 and I am so far confident in the 
library's function.

How to use
==========

OptimizationWorker is the entity testing a particular solution. 
Therefore, derive your own class from that one taking care to 
set up your problem in it's constructor and to calculate your 
fitness-function in OptimizationWorker.fitnessFunction using 
the parameters in OptimizationWorker.optParams.

Choose an appropriate MasterOptimizer, for example the ParticleSwarmOptimizer,
and call its MasterOptimizer.optimize() to start the search for a solution.

When done, retrieve best solution with MasterOptimizer.getBestParameters().

LICENSE
=======

The MIT License (MIT)

Copyright (c) 2013 Anders Bennehag
