


#ifndef STATISTICS_H_
#define STATISTICS_H_

#include <stdio.h>
#include <vector>
#include <cmath>
#include <iostream>



// How to output colour to stdout:
#define bash_red "\e[0;31m";
#define bash_RED "\e[1;31m";
#define bash_yellow "\e[0;33m";
#define bash_YELLOW "\e[1;33m";
#define bash_blue "\e[0;34m";
#define bash_BLUE "\e[1;34m";
#define bash_cyan "\e[0;36m";
#define bash_CYAN "\e[1;36m";
#define bash_NC "\e[0m"; // No Color



/** Macro generates coloured output with file and line number */
#define WARN(TO_COUT)  {std::string strfile(__FILE__);\
	strfile = strfile.substr(strfile.rfind('/')+1, std::string::npos);\
	std::cout<<"\e[0;33mWarning: "<<strfile<<":"<<__LINE__<<" - "<<	TO_COUT << "\e[0m" << std::endl;}

/** Macro generates coloured output with file and line number and then aborts program */
#define ERROR(TO_COUT)  {std::string strfile(__FILE__);\
	strfile = strfile.substr(strfile.rfind('/')+1, std::string::npos);\
	std::cout<<"\e[0;31mERROR: "<<strfile<<":"<<__LINE__<<" - "<<	TO_COUT << "\e[0m" << std::endl;\
	abort();}



/** Simple function that prints a vector */
template <class T>
void print(std::vector<T>& vec, bool vertical=1)
{
	for (unsigned i=0; i < vec.size();++i)
	{
		std::cout << i<<"\t"<<vec.at(i);
		if (vertical)
			std::cout << std::endl;
		else std::cout << " ";
	}
	if (!vertical)
		std::cout << std::endl;
};



template <class T>
T mean(std::vector<T>& vec)
{
	T sum = 0;
	for (unsigned i=0; i<vec.size(); ++i)
		sum += vec[i];

	if (vec.size()>0)
		return sum/vec.size();
	else return 0;
};

template <class T>
std::vector<T> movingAvg(std::vector<T>& in, unsigned N )
{
	std::vector<T> out;
	out.reserve( in.size() );
	double ma = 0;
	for (unsigned i=0; i < in.size(); ++i )
	{
		ma += in[i]/N;
		if (i>=N)
			ma -= in[i-N]/N;
		out.push_back(ma);
	}
	return out;
};

template <class T>
void expMovingAvg(std::vector<T>& in, double alpha, std::vector<T>& out )
{
	if (alpha<=0 || alpha >= 1)
		ERROR("Alpha outside bounds: "<<alpha);
	out.clear();
	out.reserve( in.size() );
	T ma = 0;
	for (unsigned i=0; i < in.size(); ++i )
	{
		ma = alpha*in[i] + (1-alpha)*ma;
		out.push_back(ma);
	}
};

template <class T>
std::vector<T> expMovingAvg(std::vector<T>& in, double alpha)
{
	std::vector<T> out;
	expMovingAvg(in, alpha, out);
	return out;
};



/** Calculate root squared mean error of two vectors */
template <class T>
T rmse(std::vector<T>& vec1, std::vector<T> vec2 )
{
	if (vec1.size()!=vec2.size())
		ERROR("Different size of vectors: "<<vec1.size()<<" != "<<vec2.size());

	T sq=0;
	for (unsigned i=0; i < vec1.size(); ++i)
	{
		sq += pow(vec1[i]-vec2[i],2);
	}

	return sqrt(sq);
};



/** Return sample standard deviation of vec */
template <class T>
T stddev(std::vector<T>& vec)
{
	T average = mean(vec);
	T sum = 0;
	for (unsigned i=0; i<vec.size(); ++i)
		sum += pow(vec[i]-average,2);;

	return sqrt(1.0/(vec.size()-1) * sum );
};

/** Scale data between -1 and 1 for elements within t standard deviations. */
template <class T>
void normalizeAndBound(std::vector<T>& in, int t, std::vector<T>& out, T min=-1,T max=1 )
{
	out.clear();
	out.reserve( in.size() );

	T avg = mean(in);
	T std = stddev(in);
	std *= t;

	for (unsigned i=0; i < in.size(); ++i )
	{
		if (in[i] < avg-std)
			out.push_back(min);
		else if (in[i] > avg+std)
			out.push_back(max);
		else
			out.push_back( (in[i]-avg)/(std) );
	}
};


template <class T>
void getRSI(std::vector<T>& in, int N, std::vector<T>& out )
{
	out.clear();
	out.reserve( in.size() );

	std::vector<T> U,D;
	U.reserve(in.size());D.reserve(in.size());
	U.push_back(0);D.push_back(0);

	for (unsigned i=1; i < in.size(); ++i)
	{
		T diff = in[i]-in[i-1];
		//printf("Diff: %f\n", diff);
		if (diff>0)
		{
			U.push_back(diff);
			D.push_back(0);
		}
		else
		{
			U.push_back(0);
			D.push_back(-diff);
		}
	}

	std::vector<T> Uema = expMovingAvg(U,1.0/N);
	std::vector<T> Dema = expMovingAvg(D,1.0/N);


	//printf("Uema:\n");
	//print(Uema);

	//plotTimeserie(in);
	//plotTimeserie(U, "U");
	//plotTimeserie(Uema, "expMovingAvg(U)");

	for (unsigned i=0; i < in.size(); ++i )
	{
		T RS;
		T RSI;
		if (Dema[i]>0)
		{
			RS = Uema[i]/Dema[i];
			RSI = 100 - 100/(1+RS);
		}
		else
			RSI = 100;

		out.push_back(RSI);
	}

};

template <class T>
std::vector<T> getRSI(std::vector<T>& in, int N )
{
	std::vector<T> out;
	getRSI(in, N, out);
	return out;
}

template <class T>
std::vector<T> growth(std::vector<T>& in)
{
	std::vector<T> out;
	out.reserve(in.size());
	out.push_back(1.0);

	for (unsigned i=1; i<in.size(); ++i)
		out.push_back( in[i]/in[i-1] - 1);
	return out;
};


/** Sample correlation coefficient of vectors vec1 and vec2.
 * 	More specifically, Pearsons's product-moment coefficient.*/
template <class T>
double corrcoef(std::vector<T>& vec1,std::vector<T>& vec2)
{
	if (vec1.size()!=vec2.size())
		ERROR("Different size of vectors: "<<vec1.size()<<" != "<<vec2.size());

	double mean1 = mean(vec1), mean2 = mean(vec2);
	double nom=0, denom1=0,denom2=0;
	for (unsigned i=0; i<vec1.size(); ++i)
	{
		nom += (vec1[i]-mean1)*(vec2[i]-mean2);
		denom1 += pow(vec1[i]-mean1, 2);
		denom2 += pow(vec2[i]-mean2, 2);
	}

	return nom/sqrt(denom1*denom2);
};





template <class T>
void callPythonSendData(std::vector<T>& vec, char* str_command, char* title)
{
	if (vec.size()==0)
		WARN("Size of vector for python is 0");

	FILE *in;
	in = popen(str_command, "w");
	if (in==0)
		ERROR("Could not start: "<<str_command);

	fprintf( in, "%s\n",title);
	for (unsigned i=0; i < vec.size();++i)
	{
		fprintf( in, "%f\n", vec.at(i));
	}
	pclose(in);
};

template <class T>
void plotTimeserie(std::vector<T>& vec, char* title="Timeserie")
{
	callPythonSendData(vec, "python scripts/plotTimeserie.py", title);
};

template <class T>
void plotHistogram(std::vector<T>& vec, char* title="Timeserie")
{
	callPythonSendData(vec, "python scripts/plotHistogram.py", title);
};



#endif
