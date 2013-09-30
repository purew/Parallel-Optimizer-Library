/*
 * ParticleSwarmOptimization.cpp
 *
 *  Created on: Jul 29, 2012
 *      Author: anders
 */

#include <iostream>
#include <unistd.h>
#include <thread>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <fstream>
#include <ctime>
#include <vector>

#include "Optimizer/ParticleSwarmOptimization.h"




double PAO::ParticleSwarmOptimizer::optimize()
{
	std::cout << "Optimizing " << paramBounds->size() << " dimensions"<<std::endl;
	std::cout << "Starting PSO with "<<pso.swarms<<" swarms with "<<pso.particleCount<<" particles in each and "<<pso.generations<<" generations.\n";
	switch (pso.variant)
	{
	case PopulationBest:
		std::cout << "Using population best variant\n";
		break;
	case NeighborhoodBest:
		std::cout << "Using neighborhood best variant\n";
		break;
	}
	std::cout << "Using PSO:c1="<<pso.c1<<", PSO:c2="<<pso.c2<<std::endl;

	bestParameters.fitnessValue = std::numeric_limits<double>::max();

	for (unsigned swarm=0;swarm<pso.swarms; ++swarm)
	{
		std::cout << "Initiating swarm "<<swarm+1<< " of "<<pso.swarms<<std::endl;
		unsigned params = paramBounds->size();
		std::vector<SwarmParticle> allParticles;
		allParticles.reserve(pso.particleCount);
		double inertia=0.95;

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
				newVal = randomBetween(0,1)/100 * (max-min)+min; // Todo: look over v_begin
				particle.v.push_back( newVal );
			}
			particle.x.fitnessValue =  std::numeric_limits<double>::max();
			particle.p = particle.x;

			allParticles.push_back(particle);

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

		OptimizationData swarmBestParameters;
		swarmBestParameters.fitnessValue = std::numeric_limits<double>::max();
		swarmBestParameters.parameters = allParticles.back().x.parameters;

		timespec clockStarted;
		clock_gettime(CLOCK_REALTIME,&clockStarted);

		// Start main swarm loop
		for (unsigned generation=0; generation<pso.generations;++generation)
		{
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
							+ c2*randomBetween(0,1)*(swarmBestParameters.parameters[j]-allParticles[i].x.parameters[j]);
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

			// Lower inertia for next generation
			//inertia -= 0.7/pso.generations;

			// Add particles to queue
			inmutex.lock();
			for (unsigned i=0; i<pso.particleCount; ++i)
				indataList.push_back( &(allParticles[i].x) );
			inmutex.unlock();

			notifyWorkers();

			waitUntilProcessed( allParticles.size() );

			outdataList.clear();

			// Check solutions
			for (unsigned part=0; part < allParticles.size(); ++part)
			{
				// Update particle's best location
				if (allParticles[part].x.fitnessValue < allParticles[part].p.fitnessValue )
				{
					allParticles[part].p = allParticles[part].x;
				}

				// Update population best location
				if ( allParticles[part].x.fitnessValue < swarmBestParameters.fitnessValue ) {
					swarmBestParameters = allParticles[part].x;

					if (swarmBestParameters.fitnessValue < bestParameters.fitnessValue) {
						bestParameters = swarmBestParameters;
						if (callbackFoundNewMinimum!=0)
							callbackFoundNewMinimum(bestParameters.fitnessValue, (swarm*pso.generations + generation)/((double)pso.generations * pso.swarms));
					}
				}
			}
		}
	}

	workersDone = true;
	return bestParameters.fitnessValue;
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


