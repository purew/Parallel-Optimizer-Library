
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "Optimizer.h"
#include "ParticleSwarmOptimization.h"

#include "Statistics.h"

void DumpBacktrace(int) {
	pid_t dying_pid = getpid();
	pid_t child_pid = fork();
	if (child_pid < 0) 
	{
		perror("fork() while collecting backtrace:");
	} else if (child_pid == 0) 
	{
		char buf[1024];
		sprintf(buf, "gdb -p %d -batch -ex bt 2>/dev/null | "
				"sed '0,/<signal handler/d'", dying_pid);
		const char* argv[] = {"sh", "-c", buf, NULL};
		execve("/bin/sh", (char**)argv, NULL);
		_exit(1);
	} 
	else 
	{
		waitpid(child_pid, NULL, 0);
	}
	_exit(1);
}

void BacktraceOnSegv() 
{
	struct sigaction action = {};
	action.sa_handler = DumpBacktrace;
	if (sigaction(SIGSEGV, &action, NULL) < 0) 
	{
		perror("sigaction(SEGV)");
	}
}


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
	const double L=10; // Length of dimension searched
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
	BacktraceOnSegv();
	
	const int NumRuns=100;
		
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
		psoparams.particleCount = 100;
		psoparams.generations = 100;
		psoparams.variant = PAO::NeighborhoodBest;
		
		// When instantiating a ParticleSwarmOptimzer, we supply 
		// a vector of OptimizationWorkers and the algorithm's parameters.
		PAO::ParticleSwarmOptimizer *PSO = new PAO::ParticleSwarmOptimizer( workers, psoparams );
		
		// Now all that remains is to start the optimization.
		results[i] = PSO->optimize();
		delete PSO;
	}	
	
	std::cout << "Mean is   "<<mean(results)<<std::endl;
	std::cout << "Stddev is "<<stddev(results)<<std::endl;
	
	
	return 0;
}
