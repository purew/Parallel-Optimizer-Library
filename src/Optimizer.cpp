/*
 * MasterOptimizer.cpp
 *
 *  Created on: Apr 30, 2012
 *      Author: anders
 */

#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <fstream>
#include <sys/time.h>
#include <chrono>


#include "Optimizer/Optimizer.h"
//#include "tools/Various/Common.h"


#ifndef ERROR
	/** Macro generates coloured output with file and line number */
	#define WARN(TO_COUT)  {std::string strfile(__FILE__);\
		strfile = strfile.substr(strfile.rfind('/')+1, std::string::npos);\
		std::cout<<"\e[0;33mWarning: "<<strfile<<":"<<__LINE__<<" - "<<	TO_COUT << "\e[0m" << std::endl<<std::flush;}

	#define ERROR(TO_COUT)  {std::string strfile(__FILE__);\
		strfile = strfile.substr(strfile.rfind('/')+1, std::string::npos);\
		std::cout<<"\e[0;31mERROR: "<<strfile<<":"<<__LINE__<<" - "<<	TO_COUT << "\e[0m" << std::endl<<std::flush;\
		abort();}
#endif



std::mt19937 generator;

double PAO::randomBetween( double min, double max ) 
{
	return ( ((double)generator())/generator.max() * (max-min) ) + min;
}

/*****************************************************************
 *
 * 					Class OptimizationParameters
 *
 *****************************************************************/


void PAO::ParameterBounds::registerParameter( double _min, double _max ) 
{
	min.push_back( _min );
	max.push_back( _max );
}

/*****************************************************************
 *
 * 					Class OptimizationWorker
 *
 *****************************************************************/


/** Creates a new thread and tries to acquire access to input data from masterOptimizer */
void PAO::OptimizationWorker::startWorker() 
{
	if (thrd==0)
		thrd = new std::thread(startOptimizationWorkerThread, this);
	else
		ERROR("thrd!=0");
}
void PAO::OptimizationWorker::cancelWorker() 
{
	if (thrd != 0) {
		stop = true;
		master->notifyWorkers();

		if (thrd->joinable())
			thrd->join();
		else
			std::cout << "thrd "<<thrd->get_id()<<std::endl;
	}
	else
		WARN("thrd=0");
}

PAO::OptimizationWorker::OptimizationWorker() 
{
	master=0;
	stop = false;
	thrd = 0;
}

PAO::OptimizationWorker::~OptimizationWorker() 
{
}

/** Thread does work in this function until all work is done.
	 * Responsible for locking/getting input data, starting simulation, and locking/saving output. */
void PAO::OptimizationWorker::doWork() 
{
	for (;;) {
		master->waitForStartSignal();

		for (;;) {
			std::list<OptimizationData*> dataList = master->fetchChunkOfIndata();
			if (dataList.empty()) {
				stop=true;
				break;
			}
			std::list<OptimizationData*>::iterator it;
			for (it=dataList.begin(); it!=dataList.end(); ++it) {
				// Run simulation
				double result = fitnessFunction((*it)->parameters);
				// Save fitness value
				(*it)->fitnessValue = result;
			}
			master->pushToOutdata( dataList );
		}

		if (shouldStop())
			break;
	}
}

/** Function used when starting worker in new thread. */
void* PAO::startOptimizationWorkerThread( void* pOptimizationWorker ) 
{
	OptimizationWorker* worker = (OptimizationWorker*) pOptimizationWorker;
	worker->doWork();
	return 0;
}

void PAO::OptimizationWorker::saveOptimizationParameters( std::string filename ) 
{
	std::ofstream fout(filename.c_str(),std::ios::out|std::ios::binary);
	if (fout.is_open()==0)
		ERROR("Could not open "<<filename<<" for writing")
	else
		WARN("Saving to "<<filename)

	// First write a number specifying number of parameters being saved
	fout << parameters.size() << std::endl;

	for (unsigned i=0; i<parameters.size(); ++i) 	{
		fout << parameters[i] << "\t"<< parameterBounds.min[i] <<"\t"<<  parameterBounds.max[i]<<std::endl;
	}
	fout.close();
}

void PAO::OptimizationWorker::readOptimizationParameters( std::string filename ) 
{
	std::ifstream fin(filename.c_str(), std::ios::in|std::ios::binary);
	if (fin.is_open()==0) 	{
		std::cout << "No previous parameters found at "<<filename<<std::endl;
		return;
	}

	unsigned dim;
	fin >> dim;

	// Resize parameter vectors
	parameters.resize(dim, 0);
	parameterBounds.min.resize(dim, 0);
	parameterBounds.max.resize(dim, 0);

	for (unsigned i=0; i<dim; ++i) 	{
		fin >> parameters[i] >> parameterBounds.min[i] >> parameterBounds.max[i];
	}
}

/*****************************************************************
 *
 * 					Class MasterOptimizer
 *
 *****************************************************************/


void PAO::MasterOptimizer::optimizeRandomSearch() 
{
	int steps = 200;
	int params = paramBounds->size();
	int N = steps*params;

	if (params<1)
		ERROR("params<1");

	std::vector<OptimizationData> allTestableSolutions;
	allTestableSolutions.reserve(N);

	inmutex.lock();

	for (int n=0; n<params; ++n) {
		for (int i=0;i<steps; ++i) {
			OptimizationData s;
			s.parameters.clear();

			for (int param=0; param<params; ++param) {
				double min = paramBounds->min[param];
				double max = paramBounds->max[param];
				double newVal = ((rand()%1000)/1000.0) * (max-min)+min;
				s.parameters.push_back( newVal );
			}

			allTestableSolutions.push_back(s);
			indataList.push_front( &(allTestableSolutions.back()) );
			//std::cout << allTestableSolutions[i+n*steps].params[0] << std::endl;
		}
	}
	inmutex.unlock();

	chunkSize = allTestableSolutions.size() / workers.size();

	std::cout << "indataList now contains "<<indataList.size() << " elements. Chunksize is "<<chunkSize<<std::endl;
	std::cout.flush();

	notifyWorkers();

	waitUntilProcessed( allTestableSolutions.size() );

	std::cout <<std::endl;
	// All parameters tested, load the best parameters found

	std::vector<OptimizationData>::iterator it;
	bestParameters.fitnessValue = -std::numeric_limits<double>::max();

	for (it = allTestableSolutions.begin(); it != allTestableSolutions.end(); ++it) {
		if ( (*it).fitnessValue > bestParameters.fitnessValue ) {
			bestParameters = *it;
			std::cout << "Best fitness value "<<bestParameters.fitnessValue<< std::endl;
		}
	}

	for (unsigned i=0; i<bestParameters.parameters.size();++i)
		std::cout << bestParameters.parameters[i] << " ";
	std::cout <<std::endl;
}

void PAO::MasterOptimizer::optimizeBruteforce()
{
	int steps = 100;
	int params = paramBounds->size();
	int N = steps*params;

	if (params>1)
		ERROR( "ATTENTION: Bruteforce not working for dim>1\n");

	if (params<1)
		ERROR("params<1");

	std::vector<OptimizationData> allTestableSolutions;
	allTestableSolutions.reserve(N);

	// Set up the first solution
	OptimizationData firstSolution;
	for (int i=0; i<params; ++i)
		firstSolution.parameters.push_back( paramBounds->min.at(i) );
	allTestableSolutions.push_back(firstSolution);

	int i=0;
	double newVal=0;
	double max,min;
	for (int param=0; param<params; ++param) {
		do {
			min = paramBounds->min[param];
			max = paramBounds->max[param];
			allTestableSolutions[i].parameters[param] = newVal;
			indataList.push_front( &(allTestableSolutions[i]) );
			i += 1;
			if (i>=N)
				break;
			allTestableSolutions.push_back( allTestableSolutions[i-1] );

			newVal = allTestableSolutions[i].parameters[param] + (max-min)/(steps-2);
		} while (newVal<max);
	}

	std::cout << "indataList now contains "<<indataList.size() << " elements"<<std::endl;
	std::cout.flush();

	// Now, wait for all to be done
	notifyWorkers();

	// All parameters tested, load the best parameters found
	std::vector<OptimizationData>::iterator it;
	for (it = allTestableSolutions.begin(); it != allTestableSolutions.end(); ++it) {
		if ( (*it).fitnessValue > bestParameters.fitnessValue ) {
			bestParameters = *it;
			std::cout << "Best parameter was " << bestParameters.parameters[0] << std::endl;
		}
	}
}

/** Fetch OptimizationData from the input queue.
 * 	The number of items fetched depends on number of threads. */
std::list<PAO::OptimizationData*> PAO::MasterOptimizer::fetchChunkOfIndata() 
{
	std::list<OptimizationData*> fetched;
	std::unique_lock<std::mutex> lock(inmutex);
	waitForStartSignal(lock);
	//fetched.reserve(chunkSize);
	unsigned indataSize = indataList.size();
	for ( unsigned i=0; i < indataSize && i < chunkSize; ++i ) {
		OptimizationData* indata = indataList.front();
		indataList.pop_front();
		fetched.push_back(indata);
	}
	return fetched;
}

void PAO::MasterOptimizer::pushToOutdata( std::list<OptimizationData*> &data ) 
{
	outmutex.lock();
	std::list<OptimizationData*>::iterator it = outdataList.end();
	outdataList.insert(it, data.begin(), data.end());
	outmutex.unlock();

	// Notify MasterOptimizer that some solutions are ready
	outdataReady.notify_all();
}

/** Block until all data scheduled for computation have been computed. */
void PAO::MasterOptimizer::waitUntilProcessed( unsigned itemsToProcess ) 
{
	std::unique_lock<std::mutex> lock(outmutex);
	outdataReady.wait(lock, [&] {
		return (itemsToProcess == outdataList.size()) ; });
}

PAO::MasterOptimizer::MasterOptimizer( std::vector<OptimizationWorker*> workers ) 
{
	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	generator.seed(seed);
	this->workers = workers;
	workersDone = false;
	callbackFoundNewMinimum = 0;	
	chunkSize = 1;
	paramBounds = &(workers.front()->getParameterBounds());
	if (paramBounds->size()<=0)
		ERROR("Please set appropriate parameter-bounds.\nParameterBounds->size<=0");
	
	std::cout << "\nMasterOptimizer: Using "<< workers.size() << " threads.\n";

	// Initialize the worker pool
	for (unsigned i=0; i<workers.size(); ++i)
	{		
		workers[i]->setMaster(this);
		workers[i]->startWorker();
	}
}

PAO::MasterOptimizer::~MasterOptimizer()
{
	for (unsigned i=0; i<workers.size(); ++i)
		workers[i]->cancelWorker();
}

/** Unlock indata queue so that worker threads may start computations */
void PAO::MasterOptimizer::notifyWorkers( )
{
	indataReady.notify_all();
};

/** Block until optimizing algorithm sends start signal */
void PAO::MasterOptimizer::waitForStartSignal(std::unique_lock<std::mutex> &lock)
{
	indataReady.wait(lock, [&] {
		return (indataList.size()>0 || workersDone) ; });
}

void PAO::MasterOptimizer::waitForStartSignal()
{
	std::unique_lock<std::mutex> lock(inmutex);
	waitForStartSignal(lock);
}

void PAO::MasterOptimizer::setCallbackNewMinimum( void(*fun)(double, double) )
{
	callbackFoundNewMinimum=fun;
};

/*****************************************************************
 *
 * 					Various
 *
 *****************************************************************/

void printNewMinimum(double y, double progress)
{
	printf("Progress: %.1f%%\tnew min:%f\n",progress*100,y);
}

