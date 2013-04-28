

#include "Optimizer.h"
#include "ParticleSwarmOptimization.h"


// For this example, we would like to minimize the Rosenbrock function.
// A class Rosenbrock is created to contain the details of our problem.
// The Rosenbrock class inherits PAO::OptimizationWorker which adds 
// the parallell processing capabilities.

class Rosenbrock : public PAO::OptimizationWorker
{
public:
	Rosenbrock();
	
	// PAO::OptimizationWorker requires the fitnessFunction method
	// to be implemented. This function should implement our F(X) 
	// and return the value of said function.
	double fitnessFunction (PAO::Parameters &parameters);

private:
	// Other than implementing fitnessFunction and supplying
	// parameter-bounds as seen below, there are no other requirements.
	
	const int Dimensions=3; // Dimensions of search-space
	const double L=100; // Length of dimension searched
};

double Rosenbrock::fitnessFunction(PAO::Parameters &X)
{
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
	int numWorkers = 4;	
	
	std::vector<Rosenbrock*> realWorkers;
	std::vector<PAO::OptimizationWorker*> workers;
	for (int i=0; i<numWorkers; ++i)
	{
		Rosenbrock *w = new Rosenbrock;
		realWorkers.push_back( w );
		workers.push_back( (PAO::OptimizationWorker*) w );		
	}
	
	// Here we specify som parameters for the particle swarm
	// optimization algorithm.
	PAO::PSOParameters psoparams;
	psoparams.generations = 200;
	psoparams.variant = PAO::NeighborhoodBest;
	
	// When instantiating an ParticleSwarmOptimzer, we supply 
	// a vector of OptimizationWorkers and the algorithm's parameters.
	PAO::ParticleSwarmOptimizer PSO( workers, psoparams );
	
	// Now all that remains is to start the optimization.
	double y = PSO.optimize();	
	
	return 0;
}
