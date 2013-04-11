/*
 * MasterOptimizer.cpp
 *
 *  Created on: Apr 30, 2012
 *      Author: anders
 */

#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <fstream>
#include <time.h>

#include "Optimizer.h"
//#include "tools/Various/Common.h"


#ifndef ERROR
	/** Macro generates coloured output with file and line number */
	#define WARN(TO_COUT)  {std::string strfile(__FILE__);\
		strfile = strfile.substr(strfile.rfind('/')+1, std::string::npos);\
		std::cout<<"\e[0;33mWarning: "<<strfile<<":"<<__LINE__<<" - "<<	TO_COUT << "\e[0m" << std::endl;}

	#define ERROR(TO_COUT)  {std::string strfile(__FILE__);\
		strfile = strfile.substr(strfile.rfind('/')+1, std::string::npos);\
		std::cout<<"\e[0;31mERROR: "<<strfile<<":"<<__LINE__<<" - "<<	TO_COUT << "\e[0m" << std::endl;\
		abort();}
#endif


/*****************************************************************
 *
 * 					Class OptimizationParameters
 *
 *****************************************************************/


void OptimizationParameters::registerParameter( double _param, double _min, double _max )
{
	params.push_back( _param );
	min.push_back( _min );
	max.push_back( _max );
}

/*void OptimizationParameters::putStagingParametersLive()
{
	if (stagingParameters.size() != optParams.size())
	{
		std::cerr << "ERROR: stagingParameters.size()="<<stagingParameters.size()<<" different from optParams.size()="<<optParams.size()<<std::endl;
		throw myException("Error in putStagingParametersLive()");
	}
	for (int i=0; i < optParams.size(); ++i)
	{
		*optParams[i] = stagingParameters[i];
	}
}*/


/*****************************************************************
 *
 * 					Class OptimizationWorker
 *
 *****************************************************************/


/** Creates a new thread and tries to acquire access to input data from masterOptimizer */
void OptimizationWorker::startWorker()
{
	pthread_create( &thread, NULL, startOptimizationWorkerThread, (void*)this);
	pthread_detach( thread );
}

/** Thread does work in this function until all work is done.
	 * Responsible for locking/getting input data, starting simulation, and locking/saving output. */
void OptimizationWorker::doWork()
{
	for (;;)
	{
		master->waitForStartSignal();
		for (;;)
		{

			std::vector<OptimizationData*> dataList = master->fetchChunkOfIndata();

			if (dataList.size()==0)
			{
				//std::cout << "Input data list empty. Worker thread exiting\n";
				break;
			}

			for (unsigned i=0; i < dataList.size(); ++i)
			{
				optParams.params = dataList[i]->params;

				// Run simulation
				double result = fitnessFunction();

				// Save fitness value
				dataList[i]->fitnessValue = result;

				//std::cout << "Param "<< data->params[0] << " gave fitness "<<data->fitnessValue<<std::endl;

			}

			master->pushToOutdata( dataList );
		}
	}
}

/** Function used when starting worker in new thread. */
void* startOptimizationWorkerThread( void* pOptimizationWorker )
{
	OptimizationWorker* worker = (OptimizationWorker*) pOptimizationWorker;
	worker->doWork();
	pthread_exit(0);
	return 0;
}

void OptimizationWorker::saveOptimizationParameters( std::string filename )
{
	std::ofstream fout(filename.c_str(),std::ios::out|std::ios::binary);
	if (fout.is_open()==0)
		ERROR("Could not open "<<filename<<" for writing")
	else
		WARN("Saving to "<<filename)

	// First write a number specifying number of parameters being saved
	fout << optParams.params.size() << std::endl;

	for (unsigned i=0; i<optParams.params.size(); ++i)
	{
		fout << optParams.params[i] << "\t"<< optParams.min[i] <<"\t"<<  optParams.max[i]<<std::endl;
	}
	fout.close();
}

void OptimizationWorker::readOptimizationParameters( std::string filename )
{
	std::ifstream fin(filename.c_str(), std::ios::in|std::ios::binary);
	if (fin.is_open()==0)
	{
		std::cout << "No previous parameters found at "<<filename<<std::endl;
		return;
	}

	unsigned dim;
	fin >> dim;

	// Resize parameter vectors
	optParams.params.resize(dim, 0);
	optParams.min.resize(dim, 0);
	optParams.max.resize(dim, 0);

	for (unsigned i=0; i<dim; ++i)
	{
		fin >> optParams.params[i] >> optParams.min[i] >> optParams.max[i];
	}
}

/*****************************************************************
 *
 * 					Class MasterOptimizer
 *
 *****************************************************************/


void MasterOptimizer::optimizeRandomSearch()
{
	int steps = 200;
	int params = paramBounds->size();
	int N = steps*params;

	if (params<1)
		ERROR("params<1");

	std::vector<OptimizationData> allTestableSolutions;
	allTestableSolutions.reserve(N);

	pthread_mutex_lock( &indataMutex );

	for (int n=0; n<params; ++n)
	{
		for (int i=0;i<steps; ++i)
		{
			OptimizationData s;
			s.params.clear();

			for (int param=0; param<params; ++param)
			{

				double min = paramBounds->min[param];
				double max = paramBounds->max[param];
				double newVal = ((rand()%1000)/1000.0) * (max-min)+min;
				s.params.push_back( newVal );
			}

			allTestableSolutions.push_back(s);
			indataList.push_front( &(allTestableSolutions.back()) );
			//std::cout << allTestableSolutions[i+n*steps].params[0] << std::endl;
		}
	}
	pthread_mutex_unlock( &indataMutex );

	chunkSize = allTestableSolutions.size() / threadCount;
	//if (chunkSize > 10* threadCount)
	//	chunkSize /= 5;

	std::cout << "indataList now contains "<<indataList.size() << " elements. Chunksize is "<<chunkSize<<std::endl;
	std::cout.flush();


	unlockIndata();

	waitUntilProcessed( allTestableSolutions.size() );

	std::cout <<std::endl;
	// All parameters tested, load the best parameters found

	std::vector<OptimizationData>::iterator it;
	bestParameters.fitnessValue = -std::numeric_limits<double>::max();

	for (it = allTestableSolutions.begin(); it != allTestableSolutions.end(); ++it)
	{
		//std::cout << "Fitness " << (*it).fitnessValue << std::endl;
		if ( (*it).fitnessValue > bestParameters.fitnessValue )
		{
			bestParameters = *it;
			std::cout << "Best fitness value "<<bestParameters.fitnessValue<< std::endl;
		}
	}

	for (unsigned i=0; i<bestParameters.params.size();++i)
		std::cout << bestParameters.params[i] << " ";
	std::cout <<std::endl;
}

void MasterOptimizer::optimizeBruteforce()
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
		firstSolution.params.push_back( paramBounds->min.at(i) );
	allTestableSolutions.push_back(firstSolution);

	int i=0;
	double newVal=0;
	double max,min;
	for (int param=0; param<params; ++param)
	{
		do
		{
			min = paramBounds->min[param];
			max = paramBounds->max[param];

			allTestableSolutions[i].params[param] = newVal;


			//std::cout << "i="<<i<<", newVal="<<newVal<<", param="<<param<<std::endl;

			indataList.push_front( &(allTestableSolutions[i]) );
			i += 1;
			if (i>=N)
				break;
				//throw myException("i>N",__FILE__,__LINE__);
			// Use the previous solution as template
			allTestableSolutions.push_back( allTestableSolutions[i-1] );

			newVal = allTestableSolutions[i].params[param] + (max-min)/(steps-2);
		} while (newVal<max);
	}

	std::cout << "indataList now contains "<<indataList.size() << " elements"<<std::endl;
	std::cout.flush();

	// Now, wait for all to be done
	unlockIndata();

	int unprocessedItems;
	do
	{
		usleep(1e6);
		pthread_mutex_lock( &indataMutex );
		unprocessedItems = indataList.size();
		std::cout << "indataList contains " << unprocessedItems << " untested solutions\n";
		pthread_mutex_unlock( &indataMutex );
	} while (unprocessedItems>0);

	// All parameters tested, load the best parameters found

	std::vector<OptimizationData>::iterator it;
	for (it = allTestableSolutions.begin(); it != allTestableSolutions.end(); ++it)
	{
		if ( (*it).fitnessValue > bestParameters.fitnessValue )
		{
			bestParameters = *it;
			std::cout << "Best parameter was " << bestParameters.params[0] << std::endl;
		}
	}
}

/** Fetch OptimizationData from the input queue.
 * 	The number of items fetched depends on number of threads. */
std::vector<OptimizationData*> MasterOptimizer::fetchChunkOfIndata()
{
	int ret;

	ret = pthread_mutex_lock( &indataMutex );
	if (ret!=0)
		ERROR("Problem with semaphore");

	std::vector<OptimizationData*> fetched;
	fetched.reserve(chunkSize);
	unsigned indataSize = indataList.size();

	for ( unsigned i=0; i < indataSize && i < chunkSize; ++i )
	{
		OptimizationData* indata = indataList.front();
		indataList.pop_front();
		fetched.push_back(indata);
	}

	pthread_mutex_unlock( &indataMutex );
	return fetched;
}

void MasterOptimizer::pushToOutdata( std::vector<OptimizationData*> &data )
{
	pthread_mutex_lock( &outdataMutex);
	for (unsigned i=0; i < data.size(); i++)
	{
		outdataList.push_back( data[i] );
	}
	pthread_mutex_unlock( &outdataMutex );
}

/** Block until all data scheduled for computation have been computed. */
void MasterOptimizer::waitUntilProcessed( unsigned itemsToProcess )
{
	unsigned processedItems;
	do
	{
		usleep(1e4);
		pthread_mutex_lock( &outdataMutex );
		processedItems = outdataList.size();
	//	std::cout << ".";
	//	std::cout.flush();
		pthread_mutex_unlock( &outdataMutex );
	} while (processedItems < itemsToProcess);
}

MasterOptimizer::MasterOptimizer( OptimizationWorker* _originalWorker, int _maxThreads )
{
	originalWorker = _originalWorker;
	chunkSize = 1;
	paramBounds = &(originalWorker->optParams);
	if (paramBounds->size()<=0)
		ERROR("paramBounds->size<=0 : "<<paramBounds->size());

	int ret;
	ret = pthread_mutex_init( &indataMutex, 0);
	if (ret!=0)
		ERROR("Problem with mutex:  "<<strerror(errno));

	ret = pthread_mutex_init( &outdataMutex, 0);
	if (ret!=0)
		ERROR("Problem with mutex: "<<strerror(errno));


	ret = pthread_cond_init( &beginWorkCond, 0 );
	if (ret!=0)
			ERROR("Problem with condition variable: "<<strerror(errno));

	// Find number of cores
	if (_maxThreads>0)
		threadCount = _maxThreads;
	else
		threadCount =  sysconf( _SC_NPROCESSORS_ONLN );

	std::cout << "\nMasterOptimizer: Using "<< threadCount << " threads.\n";

	// Initialize the worker pool
	for (unsigned i=0; i<threadCount; ++i)
	{
		OptimizationWorker* newWorker = originalWorker->getNewWorker();
		newWorker->setMaster(this);
		workers.push_back(newWorker);
		newWorker->startWorker();
	}
}

MasterOptimizer::~MasterOptimizer()
{
	for (unsigned i=0; i<workers.size(); ++i)
	{
		delete workers[i];
	}
	workers.clear();

	pthread_mutex_destroy(&indataMutex);
	pthread_mutex_destroy(&outdataMutex);
	pthread_cond_destroy( &beginWorkCond );
}

/** Unlock indata queue so that worker threads may start computations */
void MasterOptimizer::unlockIndata( )
{
	pthread_cond_broadcast( &beginWorkCond );
};

/** Block until optimizing algorithm sends start signal */
void MasterOptimizer::waitForStartSignal()
{
	pthread_mutex_lock( &indataMutex );
	int ret;
	bool canStart = false;
	while (!canStart)
	{
		ret = pthread_cond_wait( &beginWorkCond, &indataMutex );
		if (ret!=0)
			ERROR("pthread_cond_wait returned: "<<strerror(errno));

		if (indataList.size()>0)
		{
			canStart = true;
		}
		pthread_mutex_unlock( &indataMutex );
	}
}

