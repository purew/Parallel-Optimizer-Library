/** \mainpage Parallel Optimizer Library

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
 */


#ifndef OPTIMIZER_H_
#define OPTIMIZER_H_

#include <list>
#include <vector>
#include <string>
#include <pthread.h>

#include "Common.h"



#define LAST_OPTIMIZED_PARAMETERS_FILENAME "last_optimized_parameters"

const int ProgressUpdates = 100;

// Forward declaration
class MasterOptimizer;


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
	OptimizationWorker() {};
	virtual ~OptimizationWorker() {};

	/** Sets a MasterOptimizer for this worker.
	 * Called by MasterOptimizer*/
	void setMaster( MasterOptimizer* master) {this->master = master;};

	/** Return pointer to a new worker of same type for spawning an additional thread.
	 * 	Caller is responsible for deleting. */
	virtual OptimizationWorker* getNewWorker() = 0;

	/** Creates a new thread and tries to acquire access to input data from masterOptimizer */
	void startWorker();

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
	const ParameterBounds& getParameterBounds() {return parameterBounds;};

private:

	Parameters parameters;
	ParameterBounds parameterBounds;

	MasterOptimizer* master;
	pthread_t thread;

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
	MasterOptimizer( OptimizationWorker* originalWorker, int _maxThreads=0 );
	virtual ~MasterOptimizer();

	/** Start optimization. When done, retrieve best solution with getBestParameters() */
	virtual void optimize() = 0;

	/** Saves parameters when a new fitness high is found.
	 * 	At optimization's end, use loadBestParams() to send activate them.
	 */
	void saveBestParams();

	/** Used to load the best params found so far */
	void loadBestParams();

	/** Unlock indata queue so that worker threads may start computations */
	void unlockIndata( );

	/** Block until optimizing algorithm sends start signal */
	void waitForStartSignal();

	/** Block until all data scheduled for computation have been computed.
	 * 	\param itemsToProcess The total number of packets on indata queue. */
	void waitUntilProcessed( unsigned itemsToProcess );

	/** Fetch OptimizationData from the input queue.
	 * 	The number of items fetched depends on chunkSize */
	std::vector<OptimizationData*> fetchChunkOfIndata();
	void pushToOutdata( std::vector<OptimizationData*> &data );

	/** Returns the best solution found so far. See optimize().*/
	OptimizationData* getBestParameters() {return &bestParameters;};

protected:

	pthread_mutex_t indataMutex;
	pthread_mutex_t outdataMutex;
	pthread_cond_t beginWorkCond;

	std::vector<OptimizationWorker*> workers;
	unsigned threadCount;
	unsigned chunkSize;	//<! Size of chunk grabbed by fetchChunkOfIndata
	OptimizationWorker* originalWorker;
	ParameterBounds* paramBounds;

	OptimizationData bestParameters;


	std::list<OptimizationData*> indataList;
	std::list<OptimizationData*> outdataList;

private:
	/** Simple brute force algorithm */
	void optimizeBruteforce();
	void optimizeRandomSearch();


};




#endif /* OPTIMIZER_H_ */
