

#include "Optimizer.h"
#include "ParticleSwarmOptimization.h"

class Rosenbrock : public PAO::OptimizationWorker
{
public:
	Rosenbrock();
	double fitnessFunction (PAO::Parameters &parameters);

private:
	const int Dimensions=5;
};

double Rosenbrock::fitnessFunction(PAO::Parameters &X)
{
	double sum=0;
	for (int i=0; i<Dimensions-1; ++i)
		sum += 100*pow( X[i+1] - X[i]*X[i], 2) + pow(X[i]-1, 2);
		
	return sum;
}

Rosenbrock::Rosenbrock()
{
	PAO::ParameterBounds b;
	for (int i=0; i<Dimensions; ++i)
		b.registerParameter(-9999, 9999);
	
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
	
	PAO::PSOParameters psoparam;
	PAO::ParticleSwarmOptimizer PSO( workers, psoparam );
	
	PSO.optimize();
	
	
	return 0;
}
