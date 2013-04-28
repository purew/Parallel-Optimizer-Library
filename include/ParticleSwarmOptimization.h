/** \file

\section LICENSE

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
#ifndef PARTICLESWARMOPTIMIZATION_H_
#define PARTICLESWARMOPTIMIZATION_H_

#include "Optimizer.h"

namespace PAO
{

	enum PSOVariant_t {
		PopulationBest, ///< Population-best variant, particles tend to the population's best particle
		NeighborhoodBest ///< Neighborhood-best variant, particles tend to the neighborhood's best particle
	};

	/** Class specifying behaviour of PSO-algorithm. */
	class PSOParameters
	{
	public:
		PSOParameters() {
			this->variant = PopulationBest;
			this->particleCount=1000;
			this->generations=100;
			this->c1=0.7;
			this->c2=0.2;
		}

		PSOVariant_t variant;		///< Which type of PSO to use
		unsigned particleCount;		///< Number of particles to use
		unsigned generations;		///< Number of generations (steps) to perform
		double c1;					///< Influence of previous local best particle position
		double c2;					///< Influence of population or neighborhood best particle position
	};


	class SwarmParticle
	{
	public:
		OptimizationData x; ///< Current position of particle
		OptimizationData p; ///< Previous local best particle position
		OptimizationData* l; ///< Neighborhood best particle position
		std::vector<double> v; ///< Velocity vector

	};

	/** Implements the Particle Swarm Optimization for finding parameter-sets that 
	 *  achieve good results in the implemented fitnessFunction. Solutions are 
	 *  not guaranteed to be optimal though.
	 *
	 */
	class ParticleSwarmOptimizer : public MasterOptimizer
	{
	public:
		/** Constructor for ParticleSwarmOptimizer.
		 * \param workers The workers used in parallell during optimization. 
		 * \param parameters Contains PSO-specific parameters. */
		ParticleSwarmOptimizer( 
			std::vector<OptimizationWorker*> workers, 
			PSOParameters parameters );
			
		virtual ~ParticleSwarmOptimizer();

		double optimize();

	private:
		PSOParameters pso;
	};

}
#endif /* PARTICLESWARMOPTIMIZATION_H_ */
