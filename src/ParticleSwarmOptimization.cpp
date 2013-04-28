/*
 * ParticleSwarmOptimization.cpp
 *
 *  Created on: Jul 29, 2012
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
#include <ctime>
#include <vector>

#include "ParticleSwarmOptimization.h"




void PAO::ParticleSwarmOptimizer::optimize()
{
	double inertia=1;

	std::cout << "Starting PSO with "<<pso.particleCount<<" particles and "<<pso.generations<<" generations.\n";
	switch (pso.variant)
	{
	case PopulationBest:
		std::cout << "Using population best variant\n\n";
		break;
	case NeighborhoodBest:
		std::cout << "Using neighborhood best variant\n\n";
		break;
	}
	std::cout << "Using PSO:c1="<<pso.c1<<", PSO:c2="<<pso.c2<<std::endl;

	unsigned params = paramBounds->size();
	std::vector<SwarmParticle> allParticles;
	allParticles.reserve(pso.particleCount);

	// Set up random starting positions
	for (unsigned i=0; i<pso.particleCount; ++i)
	{
		SwarmParticle particle;

		for (unsigned param=0; param<params; ++param)
		{
			// Set up initial position
			double min = paramBounds->min[param];
			double max = paramBounds->max[param];
			double newVal = randomBetween(min, max);
			particle.x.parameters.push_back( newVal );

			// Set up initial velocity
			newVal = randomBetween(0,1)/100 * (max-min)+min;
			particle.v.push_back( newVal );
		}
		particle.x.fitnessValue =  std::numeric_limits<double>::max();
		particle.p = particle.x;

		allParticles.push_back(particle);
		indataList.push_front( &(allParticles.back().x) );

		if (pso.variant == NeighborhoodBest)
		{
			allParticles.back().l = &(allParticles.back().x);
			//std::cout <<  "Setting particle.l to "<< &(particle.x)<<""<< std::endl;
		}
	}

	chunkSize =  allParticles.size() / workers.size();
	if (chunkSize > 20*workers.size()) // Todo: cleanup
		chunkSize /= 10;
	else if (chunkSize==0)
		chunkSize+=1;

	bestParameters.fitnessValue = std::numeric_limits<double>::max();
	std::cout << "indataList now contains "<<indataList.size() << " elements. Chunksize is "<<chunkSize<<std::endl;

	timespec clockStarted;
	clock_gettime(CLOCK_REALTIME,&clockStarted);

	// Start main swarm loop
	for (unsigned i=0; i<pso.generations;++i)
	{
		/*if ((pso.generations<ProgressUpdates || i%(int)(pso.generations/(float)ProgressUpdates)==0) && i>0)
		{
			std::cout << "Starting generation "<<i<<" with inertia "<<inertia<<".\t";
			struct timespec ts,td;
			clock_gettime(CLOCK_REALTIME,&ts);
			td = diff(clockStarted,ts);
			double t = td.tv_sec+td.tv_nsec/1e9;
			double timeLeft = t/i*(pso.generations-i);
			std::cout << "Time left: "<< (int)timeLeft/60 <<" min and "<<(int)timeLeft%60<<" seconds\n";
			std::cout.flush();
		}*/

		unlockIndata();

		waitUntilProcessed( allParticles.size() );

		outdataList.clear();

		// Check solutions
		for (unsigned i=0; i < allParticles.size(); ++i)
		{
			// Update particle's best location
			if (allParticles[i].x.fitnessValue < allParticles[i].p.fitnessValue )
			{
				allParticles[i].p = allParticles[i].x;
			}

			// Update population best location
			if ( allParticles[i].x.fitnessValue < bestParameters.fitnessValue )
			{
				bestParameters = allParticles[i].x;
				std::cout << "Best fitness value "<<bestParameters.fitnessValue<< std::endl;
			}
		}

		if (pso.variant == NeighborhoodBest)
		{
			for (unsigned i=0; i < allParticles.size(); ++i)
			{
				// Update neighborhood best location

				// Use ring topology
				unsigned prev = (i-1+allParticles.size())%allParticles.size();
				unsigned next = (i+1+allParticles.size())%allParticles.size();
				if ( allParticles[next].p.fitnessValue < allParticles[i].p.fitnessValue )
				{
					allParticles[i].l = &(allParticles[next].p);
					//std::cout << "Particle "<<i<<" found new neighborhood max "<<allParticles[i].l->fitnessValue <<std::endl;
					//std::cout.flush();
				}
				if ( allParticles[prev].p.fitnessValue < allParticles[i].p.fitnessValue )
				{
					allParticles[i].l = &(allParticles[prev].p);
					//std::cout << "Particle "<<i<<" found new neighborhood max "<<allParticles[i].l->fitnessValue <<std::endl;
					//std::cout.flush();
				}
			}
		}

		double c1=pso.c1,c2=pso.c2;

		// Update particle locations
		for (unsigned i=0; i<pso.particleCount; ++i)
		{
			for (unsigned j=0; j<params; ++j)
			{
				switch (pso.variant)
				{
				case NeighborhoodBest:
					allParticles[i].v[j] = allParticles[i].v[j]*inertia
						+ c1*randomBetween(0,1)*(allParticles[i].p.parameters[j]-allParticles[i].x.parameters[j])
						+ c2*randomBetween(0,1)*(allParticles[i].l->parameters[j]-allParticles[i].x.parameters[j]);
					break;
				case PopulationBest:
					allParticles[i].v[j] = allParticles[i].v[j]*inertia
						+ c1*randomBetween(0,1)*(allParticles[i].p.parameters[j]-allParticles[i].x.parameters[j])
						+ c2*randomBetween(0,1)*(bestParameters.parameters[j]-allParticles[i].x.parameters[j]);
					break;
				}

				double newPos = allParticles[i].x.parameters[j] + allParticles[i].v[j];
				if (newPos < paramBounds->min[j])
					newPos = paramBounds->min[j];
				if (newPos > paramBounds->max[j])
					newPos = paramBounds->max[j];

				allParticles[i].x.parameters[j] = newPos;
			}
		}

		// Add particles to queue
		pthread_mutex_lock( &indataMutex );
		for (unsigned i=0; i<pso.particleCount; ++i)
			indataList.push_back( &(allParticles[i].x) );
		pthread_mutex_unlock( &indataMutex );

		// Lower inertia for next generation
		inertia -= 0.7/pso.generations;
	}
}



PAO::ParticleSwarmOptimizer::ParticleSwarmOptimizer( 
	std::vector<PAO::OptimizationWorker*> workers, 
	PAO::PSOParameters parameters 
	)
 : MasterOptimizer( workers )
{
	pso = parameters;
}

PAO::ParticleSwarmOptimizer::~ParticleSwarmOptimizer()
{

}



