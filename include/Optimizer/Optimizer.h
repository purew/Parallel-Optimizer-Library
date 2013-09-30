/** \mainpage Parallel Optimizer Library

Introduction
============

Parallel Optimizer Library searches for the *X* that minimizes *f(X)*,
preferably by using evolutionary algorithms and multithreading capabilities.

This library was created when I found myself needing a way of finding 
good solutions to a multi-dimensional problem where exhaustive search
is infeasible due to the large search-space.

It is implemented using *C++11* and its native multithreading capabilities.
My plan is to also implement *MPI* which would make this library
far more useful for computer clusters.

The library is evolving and the API should not be considered final in any way yet. However, I have been using this library for my own application since early 2012 and I am so far confident in the library's function.

How to use
==========

OptimizationWorker is the entity testing a particular solution. 
Therefore, derive your own class from that one, taking care to
set up your problem in its constructor and to calculate your
fitness-function in OptimizationWorker.fitnessFunction.

Choose an appropriate MasterOptimizer, for example the ParticleSwarmOptimizer,
and call its MasterOptimizer.optimize() to start the search for a solution.

When done, retrieve best solution with MasterOptimizer.getBestParameters().

Example
=======

There is an example implementation of the Rosenbrock-problem in 
example/rosenbrock.cpp. Build the example code with waf using

	./waf build_release

The compiled binary is found at build/release/rosenbrock

Documentation
=============

Documentation is generated by doxygen through

	doxygen

Todo
====

- MPI-support
- Auto-tuning of PSO-swarm.
- Exhaustive search for reference.
- Add regression tests
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

END OF DOCUMENTATION
 */


#ifndef OPTIMIZER_H_
#define OPTIMIZER_H_

#include <list>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <random>
#include <iostream>

namespace PAO
{

	#define LAST_OPTIMIZED_PARAMETERS_FILENAME "last_optimized_parameters"

	const int ProgressUpdates = 100;

	// Forward declaration
	class MasterOptimizer;


	/** Return a random number in (min,max) */
	double randomBetween( double min, double max );


	/*****************************************************************
	 *
	 * 					Class OptimizationParameters
	 *
	 *****************************************************************/

	/** Class for handling parameters used in optimization. */
	class ParameterBounds
	{
	public:
		ParameterBounds() {};
		~ParameterBounds() {};

		/** For a new object, register its parameters with registerOptParam. */
		void registerParameter( double min, double max );
		/** Returns number of parameters in use */
		int size() {return max.size();};

		std::vector<double> max;
		std::vector<double> min;

	private:
	};

	/** The parameters for fitness-function, or the X in f(X). 
	 * 	At the moment implemented as an std::vector<double>. */
	class Parameters : public std::vector<double>
	{

	};

	/** Structure holding parameters and their corresponding fitness-value*/
	class OptimizationData
	{
	public:
		Parameters parameters; ///< Parameters used in simulation.
		double fitnessValue;		///< Fitness-value associated with parameters.
	};



	/*****************************************************************
	 *
	 * 					Class OptimizationWorker
	 *
	 *****************************************************************/

	/** Classes wishing to be optimized need to inherit this class. */
	class OptimizationWorker
	{
	public:
		/** Fitness function used for evaluating parameters.
		 *  A higher value is better. Derived classes need to implement
		 *  fitnessFunction.
		 *  \param parameters Contains parameters used in fitness function. */
		virtual double fitnessFunction(Parameters &parameters) = 0;

		/** Constructor is executed once per worker and should be
		 * 	used for preprocessing and initializing data.
		 * 	Each worker lives in it's own thread.
		 */
		OptimizationWorker();
		virtual ~OptimizationWorker();

		/** Sets a MasterOptimizer for this worker.
		 * Called by MasterOptimizer*/
		void setMaster( MasterOptimizer* master) {this->master = master;};

		/** Return pointer to a new worker of same type for spawning an additional thread.
		 * 	Caller is responsible for deleting. */
		//virtual OptimizationWorker* getNewWorker();

		/** Creates a new thread and tries to acquire access to input data from masterOptimizer */
		void startWorker();
		/** Sends a cancellation request to thread running OptimizationWorker.
		 *  Destructor is called during this call. */
		void cancelWorker();
		/** Returns true if worker is scheduled to stop and exit thread. */
		bool shouldStop() {return stop;};

		/** Thread does work in this function until all work is done.
		 * Responsible for locking/getting input data, starting simulation, and locking/saving output. */
		void doWork();

		/** Save current parameters to file 
		 * @param filename Where to save parameters */
		void saveOptimizationParameters( std::string filename = std::string(LAST_OPTIMIZED_PARAMETERS_FILENAME) );
		/** Load current parameters from file 
		 * @param filename Load parameters from this place */
		void readOptimizationParameters( std::string filename = std::string(LAST_OPTIMIZED_PARAMETERS_FILENAME) );

		/** Set current parameters */
		void setParameters( Parameters& parameters ) {this->parameters = parameters;};
		/** Get current parameters */
		const Parameters& getParameters() {return parameters;};
		/** Set current parameter bounds */
		void setParameterBounds( ParameterBounds parameterBounds ) {this->parameterBounds=parameterBounds;};
		/** Get current parameter bounds */
		ParameterBounds& getParameterBounds() {return parameterBounds;}; //Todo: make return const

	private:

		Parameters parameters;
		ParameterBounds parameterBounds;

		MasterOptimizer* master;
		std::thread *thrd;
		std::atomic<bool> stop;
	};

	/* Function used when starting worker in new thread.
	 * Todo: Should not be public*/
	void* startOptimizationWorkerThread( void* pOptimizationWorker );



	/*****************************************************************
	 *
	 * 					Class MasterOptimizer
	 *
	 *****************************************************************/


	/** Entity responsible for setting up worker-threads and dividing work among them.
	 * MasterOptimizer contains the virtual optimize() which contains the optimization algorithm.
	 * This function is implemented in by derived classes such as ParticleSwarmOptimizer
	 *
	 */
	class MasterOptimizer
	{
	public:
		/** Takes max number of threads to spawn as argument.
		 *  Defaults to 0 which will use as many threads as there are cores
		 *  according to sysconf.*/
		MasterOptimizer( std::vector<OptimizationWorker*> workers );
		virtual ~MasterOptimizer();

		/** Start optimization of OptimizationWorker.fitnessFunction.
		 *  When done, retrieve best solution with getBestParameters().
		 *  \return Best value of fitnessFunction found. */
		virtual double optimize() = 0;

		/** Save best parameters found after calling optimize() to file.
		 *  Todo: Let user pick filename */
		void saveBestParams();

		/** Load parameters saved with saveBestParams()
		*   Todo: Let user pick filename*/
		void loadBestParams();

		/** Notify workers */
		void notifyWorkers();

		/** Block until optimizing algorithm sends start signal */
		void waitForStartSignal(std::unique_lock<std::mutex> &lock);
		void waitForStartSignal();


		/** Block until all data scheduled for computation have been computed.
		 * 	\param itemsToProcess The total number of packets on indata queue.*/
		void waitUntilProcessed( unsigned itemsToProcess );

		/** Fetch OptimizationData from the input queue.
		 * 	The number of items fetched depends on chunkSize */
		std::list<PAO::OptimizationData*> fetchChunkOfIndata();
		void pushToOutdata( std::list<OptimizationData*> &data );

		/** Returns the best solution found so far. See optimize().*/
		OptimizationData* getBestParameters() {return &bestParameters;};

		/** Register function called when a new minimum is found.
		 * 	Could be used for printout of search-progress.
		 * 	\param fun Function handle taking new minimum y and progress as arguments. progress is between 0 and 1.0.
		 */
		void setCallbackNewMinimum(void(*fun)(double y, double progress ));;

	protected:

		std::mutex inmutex;
		std::mutex outmutex;
		std::atomic<bool> workersDone;

		std::vector<OptimizationWorker*> workers;
		unsigned chunkSize;	//<! Size of chunk grabbed by fetchChunkOfIndata
		OptimizationWorker* originalWorker;
		ParameterBounds* paramBounds;

		OptimizationData bestParameters;


		std::list<OptimizationData*> indataList;
		std::list<OptimizationData*> outdataList;

		void (*callbackFoundNewMinimum)(double y, double progress );

	private:
		std::condition_variable indataReady;
		std::condition_variable outdataReady;


		/** Simple brute force algorithm */
		void optimizeBruteforce();
		void optimizeRandomSearch();

	};
}

/** Simple function that prints current best minimum and progress for use with setCallbackNewMinimum() */
void printNewMinimum(double y, double progress);

#endif /* OPTIMIZER_H_ */
