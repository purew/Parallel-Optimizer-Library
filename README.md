
Introduction
============

This library was created when I found myself needing a way of finding 
good solutions to a multi-dimensional problem where exhaustive search
is impossible due to the large search-space. 

It is implemented using C++ and pthreads and is POSIX-compliant. My plans are
to also implement OpenMPI which would make this library far more 
useful for computer clusters.

It is evolving and the API should not be considered final in any way yet.
However, I have been using this library for my own application since early 2012
and I am so far confident in the library's function.

How to use
==========

OptimizationWorker is the entity testing a particular solution. 
Therefore, derive your own class from that one, taking care to
set up your problem in its constructor and to calculate your
fitness-function in OptimizationWorker.fitnessFunction using 
the parameters from OptimizationWorker.getParameters().

Choose an appropriate MasterOptimizer, for example the ParticleSwarmOptimizer,
and call its MasterOptimizer.optimize() to start the search for a solution.

When done, retrieve best solution with MasterOptimizer.getBestParameters().

Example
=======

There is an example implementation of the Rosenbrock-problem in 
example/rosenbrock.cpp which is built with CMake using

	mkdir Debug
	cd Debug
	cmake ..
	make

Or by using the helper-script 

	./runcmake debug 	or
	./runcmake release
	

Todo
====

- Auto-tuning of PSO-swarm.
- Exhaustive search for reference.
- Implement a way for user to disable and/or customize what library prints to stdout.

 
LICENSE
=======

The MIT License (MIT)

Copyright (c) 2013 Anders Bennehag

Permission is hereby granted, free of charge, to any person obtaining 
a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without 
limitation the rights to use, copy, modify, merge, publish, 
distribute, sublicense, and/or sell copies of the Software, 
and to permit persons to whom the Software is furnished to do so, 
subject to the following conditions:

The above copyright notice and this permission notice shall be 
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR 
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF 
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

