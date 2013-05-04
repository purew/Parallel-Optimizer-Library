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
#include <sys/time.h>
#include <chrono>


#include "Optimizer.h"
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
	int err =pthread_create( &thread, 0, startOptimizationWorkerThread, (void*)this);
	if (err)
		ERROR("Could not spawn thread");
}
void PAO::OptimizationWorker::cancelWorker()
{
	if (thread != 0)
	{
		stop = true;
		master->unlockIndata();

		int err = pthread_join( thread, 0 );
		if (err)
		{
			std::cout << EDEADLK <<std::endl <<EINVAL <<std::endl <<ESRCH <<std::endl;
			ERROR("Could not join thread "<<thread<<". Error: "<<err<<" "<<"\""<<strerror(err)<<"\"");
		}
	}
	else
		ERROR("Thread not initialized yet");
}

PAO::OptimizationWorker::OptimizationWorker()
{
	master=0;
	thread=0;
	stop = false;
}

PAO::OptimizationWorker::~OptimizationWorker()
{
	cancelWorker();
}

/** Thread does work in this function until all work is done.
	 * Responsible for locking/getting input data, starting simulation, and locking/saving output. */
void PAO::OptimizationWorker::doWork()
{
	for (;;)
	{
		master->waitForStartSignal( this );

		if (shouldStop())
			break;

		for (;;)
		{

			std::vector<OptimizationData*> dataList = master->fetchChunkOfIndata();

			if (dataList.empty())
				break;

			for (unsigned i=0; i < dataList.size(); ++i)
			{
				parameters = dataList[i]->parameters;

				// Run simulation
				double result = fitnessFunction(parameters);

				// Save fitness value
				dataList[i]->fitnessValue = result;

				//std::cout << "Param "<< data->params[0] << " gave fitness "<<data->fitnessValue<<std::endl;

			}

			master->pushToOutdata( dataList );
		}
	}
}

/** Function used when starting worker in new thread. */
void* PAO::startOptimizationWorkerThread( void* pOptimizationWorker )
{
	OptimizationWorker* worker = (OptimizationWorker*) pOptimizationWorker;
	worker->doWork();
	pthread_exit(0);
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

	for (unsigned i=0; i<parameters.size(); ++i)
	{
		fout << parameters[i] << "\t"<< parameterBounds.min[i] <<"\t"<<  parameterBounds.max[i]<<std::endl;
	}
	fout.close();
}

void PAO::OptimizationWorker::readOptimizationParameters( std::string filename )
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
	parameters.resize(dim, 0);
	parameterBounds.min.resize(dim, 0);
	parameterBounds.max.resize(dim, 0);

	for (unsigned i=0; i<dim; ++i)
	{
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

	int ret = pthread_mutex_lock( &indataMutex );
	if (ret!=0)
		ERROR("Problem with mutex");

	for (int n=0; n<params; ++n)
	{
		for (int i=0;i<steps; ++i)
		{
			OptimizationData s;
			s.parameters.clear();

			for (int param=0; param<params; ++param)
			{

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
	ret = pthread_mutex_unlock( &indataMutex );
	if (ret!=0)
		ERROR("Problem with mutex");

	chunkSize = allTestableSolutions.size() / workers.size();

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
	for (int param=0; param<params; ++param)
	{
		do
		{
			min = paramBounds->min[param];
			max = paramBounds->max[param];

			allTestableSolutions[i].parameters[param] = newVal;


			//std::cout << "i="<<i<<", newVal="<<newVal<<", param="<<param<<std::endl;

			indataList.push_front( &(allTestableSolutions[i]) );
			i += 1;
			if (i>=N)
				break;
				//throw myException("i>N",__FILE__,__LINE__);
			// Use the previous solution as template
			allTestableSolutions.push_back( allTestableSolutions[i-1] );

			newVal = allTestableSolutions[i].parameters[param] + (max-min)/(steps-2);
		} while (newVal<max);
	}

	std::cout << "indataList now contains "<<indataList.size() << " elements"<<std::endl;
	std::cout.flush();

	// Now, wait for all to be done
	unlockIndata();

	int unprocessedItems;
	do
	{
		usleep(1e6); // Todo: fix 
		int ret = pthread_mutex_lock( &indataMutex );
		if (ret!=0)
			ERROR("Problem with mutex");

		unprocessedItems = indataList.size();
		std::cout << "indataList contains " << unprocessedItems << " untested solutions\n";

		ret = pthread_mutex_unlock( &indataMutex );
		if (ret!=0)
			ERROR("Problem with mutex");
	} while (unprocessedItems>0);

	// All parameters tested, load the best parameters found

	std::vector<OptimizationData>::iterator it;
	for (it = allTestableSolutions.begin(); it != allTestableSolutions.end(); ++it)
	{
		if ( (*it).fitnessValue > bestParameters.fitnessValue )
		{
			bestParameters = *it;
			std::cout << "Best parameter was " << bestParameters.parameters[0] << std::endl;
		}
	}
}

/** Fetch OptimizationData from the input queue.
 * 	The number of items fetched depends on number of threads. */
std::vector<PAO::OptimizationData*> PAO::MasterOptimizer::fetchChunkOfIndata()
{
	int ret=0;

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
	ret = pthread_mutex_unlock( &indataMutex );
	if (ret!=0)
		ERROR("Problem with mutex");
	return fetched;
}

void PAO::MasterOptimizer::pushToOutdata( std::vector<OptimizationData*> &data )
{
	pthread_mutex_lock( &outdataMutex);
	for (unsigned i=0; i < data.size(); i++)
	{
		outdataList.push_back( data[i] );
	}
	pthread_mutex_unlock( &outdataMutex );
}

/** Block until all data scheduled for computation have been computed. */
void PAO::MasterOptimizer::waitUntilProcessed( unsigned itemsToProcess )
{
	unsigned processedItems;
	do
	{
		usleep(1e4); // Todo: get rid of sleep
		pthread_mutex_lock( &outdataMutex );
		processedItems = outdataList.size();
	//	std::cout << ".";
	//	std::cout.flush();
		pthread_mutex_unlock( &outdataMutex );
	} while (processedItems < itemsToProcess);
}

PAO::MasterOptimizer::MasterOptimizer( std::vector<OptimizationWorker*> workers )
{
	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	generator.seed(seed);

	this->workers = workers;
	
	chunkSize = 1;
	paramBounds = &(workers.front()->getParameterBounds());
	if (paramBounds->size()<=0)
		ERROR("Please set appropriate parameter-bounds.\nParameterBounds->size<=0");

	int ret;
	ret = pthread_mutex_init( &indataMutex, 0);
	if (ret!=0)
		ERROR("Problem with mutex:  "<<strerror(ret));

	ret = pthread_mutex_init( &outdataMutex, 0);
	if (ret!=0)
		ERROR("Problem with mutex: "<<strerror(ret));


	ret = pthread_cond_init( &beginWorkCond, 0 );
	if (ret!=0)
			ERROR("Problem with condition variable: "<<strerror(ret));

	// Find number of cores
	/*if (maxThreads>0)
		threadCount = maxThreads;
	else
		threadCount =  sysconf( _SC_NPROCESSORS_ONLN );
		*/
	
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
	{
		delete workers[i];
	}
	workers.clear();

	pthread_mutex_destroy(&indataMutex);
	pthread_mutex_destroy(&outdataMutex);
	pthread_cond_destroy( &beginWorkCond );
}

/** Unlock indata queue so that worker threads may start computations */
void PAO::MasterOptimizer::unlockIndata( )
{
	pthread_cond_broadcast( &beginWorkCond );
};

/** Block until optimizing algorithm sends start signal */
void PAO::MasterOptimizer::waitForStartSignal( PAO::OptimizationWorker *caller )
{
	int ret = pthread_mutex_lock( &indataMutex );
	if (ret!=0)
		ERROR("Problem with mutex");
	
	bool canStart = false;
	while (!canStart)
	{
		ret = pthread_cond_wait( &beginWorkCond, &indataMutex );

		if (ret!=0)
			ERROR("pthread_cond_wait returned: "<<ret<<" \""<<strerror(ret)<<"\"");

		if ( indataList.size()>0
			|| caller->shouldStop() )
			break;
	}

	ret = pthread_mutex_unlock( &indataMutex );
	if (ret!=0)
		ERROR("Problem with mutex");
}

