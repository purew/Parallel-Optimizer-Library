
#include <atomic>

#include "Optimizer/Optimizer.h"
#include "Optimizer/ParticleSwarmOptimization.h"

#include "Statistics.h"


/**
 *  For an example on how to use Parallel Optimizer Library,
 *  have a look in example/rosenbrock.cpp.
 *
 * 	This file is used for development and evaluation of Parallel Optimizer Library.
 * 	It may or may not be useful to other people.
 */



class Rosenbrock : public PAO::OptimizationWorker
{
public:
	Rosenbrock();
	
	// PAO::OptimizationWorker requires the fitnessFunction method
	// to be implemented. This function should implement our F(X) 
	// and return the value of said function.
	double fitnessFunction (PAO::Parameters &parameters);

	static int nbrEvaluations() {return evaluations;};

private:
	// Other than implementing fitnessFunction and supplying
	// parameter-bounds as seen below, there are no other requirements.
	
	const int Dimensions=5; // Dimensions of search-space
	const double L=10; // Length of dimension searched

	static std::atomic<int> evaluations;
};

std::atomic<int> Rosenbrock::evaluations(0);

double Rosenbrock::fitnessFunction(PAO::Parameters &X)
{
	evaluations += 1;
	double sum=0;
	for (int i=0; i<Dimensions-1; ++i)
		sum += 100*pow( X[i+1] - X[i]*X[i], 2) + pow(X[i]-1, 2);
		
	return sum;
}

// In the constructor we use setParameterBounds to describe the
// parameter-space we would like to search:
Rosenbrock::Rosenbrock()
{
	PAO::ParameterBounds b;
	for (int i=0; i<Dimensions; ++i)
		b.registerParameter(-L/2, L/2);
	
	setParameterBounds(b);
}

int main()
{
	const int NumRuns=10;
		
	std::vector<double> results(NumRuns);
	for (int i=0; i<NumRuns; ++i)
	{
		std::cout << "Starting iteration "<<i<<std::endl;
		
		int numWorkers = 4;	
		
		// The optimizer takes a vector of OptimizationWorker*'s as argument
		// where each worker runs in parallell during optimization.
		// Here we create that vector:
		std::vector<PAO::OptimizationWorker*> workers;
		for (int i=0; i<numWorkers; ++i)
		{
			Rosenbrock *w = new Rosenbrock;
			workers.push_back( (PAO::OptimizationWorker*) w );		
		}
		
		// Here we specify some parameters for the particle swarm
		// optimization algorithm.
		PAO::PSOParameters psoparams;
		psoparams.particleCount = 1000;
		psoparams.generations = 100;
		psoparams.variant = PAO::NeighborhoodBest;
		
		// When instantiating a ParticleSwarmOptimzer, we supply 
		// a vector of OptimizationWorkers and the algorithm's parameters.
		PAO::ParticleSwarmOptimizer *PSO = new PAO::ParticleSwarmOptimizer( workers, psoparams );
		
		// Now all that remains is to start the optimization.
		double y = results[i] = PSO->optimize();

		std::cout << "Result is "<<y<<std::endl;

		// Cleanup
		delete PSO;
		for (int i=0; i<numWorkers; ++i)
			delete workers[i];
	}	
	
	double minY = std::numeric_limits<double>::max();
	for (unsigned i=0; i<results.size(); ++i)
		if (results[i] < minY)
			minY = results[i];

	std::cout << "\n*************************\n";
	std::cout << "Performed "<<NumRuns<<" optimizations and "<<Rosenbrock::nbrEvaluations()/(float)NumRuns<<" function evaluations in each"<<std::endl;
	std::cout << "Best is   "<<minY<<std::endl;
	std::cout << "Mean is   "<<mean(results)<<std::endl;
	std::cout << "Stddev is "<<stddev(results)<<std::endl;
	
	
	return 0;
}
