#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include "Simulator.h"

using namespace std;

//need rand in like the range [0,1) for the exp dist so yea cus yea
double uniform_rand() {
    //Static Cast :O!!!
    //return static_cast<double>(rand())/(static_cast<double>(RAND_MAX) + 1.0);
    return drand48();
}

// exp dist helper, redo over and over untill in the bounds thats less than upper
double next_exp(double lambda, int upperBound) {
    double x;
    double r = uniform_rand();
    while (r == 0.0){
        r = uniform_rand();
    }
    x = -log(r)/lambda;

    while (x > upperBound){
        double r = uniform_rand();

        while (r == 0.0){
            r = uniform_rand();
        }
        
        x = -log(r)/lambda;
        //cout<<x<<endl;
    } 

    return x;
}

//make ids  A0, A1, ... A9, B0, B1,.... Z1 , Z2,...,Z9 
string makeProcessId(int index) {
    char letter = 'A' + (index / 10);
    int digit = index % 10;

    return string(1, letter) + to_string(digit);
}

// print one process in the summary format
void printProcess(const Process& p) {
    const vector<int>& cpu = p.getCpuBursts();

    if (p.isIoBound()){
        cout<<"I/O-bound process "<<p.getId()<<": arrival time "<<p.getArrivalTime()<<"ms; "<<cpu.size()<<" "<<(cpu.size() == 1 ? "CPU burst" : "CPU bursts")<<":"<<endl;
    }else{
        cout<<"CPU-bound process "<<p.getId()<<": arrival time "<<p.getArrivalTime()<<"ms; "<<cpu.size()<<" "<<(cpu.size() == 1 ? "CPU burst" : "CPU bursts")<<":"<<endl;
    }
}

double computeAverage(long long total, int count) {
    if (count == 0) {
        return 0.0;
    }

    return static_cast<double>(total) / static_cast<double>(count);
}



//the file shit
void writeAlgorithmStats(ofstream& simout, const string& algorithm, const simulator::AlgorithmStats& stats) {
    simout<<"Algorithm "<<algorithm<<endl;
    simout<<"-- CPU utilization: "<<stats.cpuUtilization<<"%"<<endl;
    simout<<"-- CPU-bound average wait time: "<<stats.cpuBoundAverageWait<<" ms"<<endl;
    simout<<"-- I/O-bound average wait time: "<<stats.ioBoundAverageWait<<" ms"<<endl;
    simout<<"-- overall average wait time: "<<stats.overallAverageWait<<" ms"<<endl;
    simout<<"-- CPU-bound average turnaround time: "<<stats.cpuBoundAverageTurnaround<<" ms"<<endl;
    simout<<"-- I/O-bound average turnaround time: "<<stats.ioBoundAverageTurnaround<<" ms"<<endl;
    simout<<"-- overall average turnaround time: "<<stats.overallAverageTurnaround<<" ms"<<endl;
    simout<<"-- CPU-bound number of context switches: "<<stats.cpuBoundContextSwitches<<endl;
    simout<<"-- I/O-bound number of context switches: "<<stats.ioBoundContextSwitches<<endl;
    simout<<"-- overall number of context switches: "<<stats.overallContextSwitches<<endl;
    simout<<"-- CPU-bound number of preemptions: "<<stats.cpuBoundPreemptions<<endl;
    simout<<"-- I/O-bound number of preemptions: "<<stats.ioBoundPreemptions<<endl;
    simout<<"-- overall number of preemptions: "<<stats.overallPreemptions<<endl;
    simout<<"-- CPU-bound percentage of CPU bursts completed within one time slice: "<<stats.cpuBoundOneSlicePercent<<"%"<<endl;
    simout<<"-- I/O-bound percentage of CPU bursts completed within one time slice: "<<stats.ioBoundOneSlicePercent<<"%"<<endl;
    simout<<"-- overall percentage of CPU bursts completed within one time slice: "<<stats.overallOneSlicePercent<<"%"<<endl;
}

void writeSimout(const vector<Process>& processes, int cpuBoundCount, const simulator::AlgorithmStats& rrStats) {
    //open fil
    ofstream simout("simout.txt");

    long long cpuBoundCpuTotal = 0;
    long long ioBoundCpuTotal = 0;
    long long cpuBoundIoTotal = 0;
    long long ioBoundIoTotal = 0;
    int cpuBoundCpuCount = 0;
    int ioBoundCpuCount = 0;
    int cpuBoundIoCount = 0;
    int ioBoundIoCount = 0;

    // loop through all processes and add up the totals and counts for cpu and io bursts for both cpu-bound and io-bound processes
    for (const Process& process : processes) {
        const vector<int>& cpuBursts = process.getCpuBursts();
        const vector<int>& ioBursts = process.getIoBursts();

        if (process.isIoBound()) {
            for (int burst : cpuBursts) {
                ioBoundCpuTotal += burst;
                ioBoundCpuCount++;
            }

            for (int burst : ioBursts) {
                ioBoundIoTotal += burst;
                ioBoundIoCount++;
            }
        } else {
            for (int burst : cpuBursts) {
                cpuBoundCpuTotal += burst;
                cpuBoundCpuCount++;
            }

            for (int burst : ioBursts) {
                cpuBoundIoTotal += burst;
                cpuBoundIoCount++;
            }
        }
    }

    int ioBoundCount = static_cast<int>(processes.size()) - cpuBoundCount;
    long long overallCpuTotal = cpuBoundCpuTotal + ioBoundCpuTotal;
    long long overallIoTotal = cpuBoundIoTotal + ioBoundIoTotal;
    int overallCpuCount = cpuBoundCpuCount + ioBoundCpuCount;
    int overallIoCount = cpuBoundIoCount + ioBoundIoCount;

    //print this stuff to simout.txt 
    simout<<fixed<<setprecision(2);
    simout<<"-- number of processes: "<<processes.size()<<endl;
    simout<<"-- number of CPU-bound processes: "<<cpuBoundCount<<endl;
    simout<<"-- number of I/O-bound processes: "<<ioBoundCount<<endl;
    simout<<"-- CPU-bound average CPU burst time: "<<computeAverage(cpuBoundCpuTotal, cpuBoundCpuCount)<<" ms"<<endl;
    simout<<"-- I/O-bound average CPU burst time: "<<computeAverage(ioBoundCpuTotal, ioBoundCpuCount)<<" ms"<<endl;
    simout<<"-- overall average CPU burst time: "<<computeAverage(overallCpuTotal, overallCpuCount)<<" ms"<<endl;
    simout<<"-- CPU-bound average I/O burst time: "<<computeAverage(cpuBoundIoTotal, cpuBoundIoCount)<<" ms"<<endl;
    simout<<"-- I/O-bound average I/O burst time: "<<computeAverage(ioBoundIoTotal, ioBoundIoCount)<<" ms"<<endl;
    simout<<"-- overall average I/O burst time: "<<computeAverage(overallIoTotal, overallIoCount)<<" ms"<<endl;
    writeAlgorithmStats(simout, "RR", rrStats);
}

//generate one  process
Process generateProcess(int index, bool isIoBound, double lambda, int upperBound){


    //make the process using the class thiny in .h
    Process p(makeProcessId(index), isIoBound);

    //arrival time == floor of nxtExp with lambda and upper bound
    int arrival = static_cast<int>(floor(next_exp(lambda, upperBound)));
    p.setArrivalTime(arrival);

    //number of cpu bursts should be in range [1,16] based on the univform rand * 16 and then ceil it to get the int value in the range
    int numCpuBursts = 0;
    while(numCpuBursts < 1 || numCpuBursts > 16){
        numCpuBursts = static_cast<int>(ceil(uniform_rand()*16.0));
    }

    for(int burst = 0; burst < numCpuBursts; burst++){
        int cpuTime = static_cast<int>(ceil(next_exp(lambda, upperBound)));

        //cpu bound mean *6
        if (!isIoBound) {
            cpuTime *= 6;
        }

        p.addCpuBurst(cpuTime);

        //ast cpu burst does not get an io burst so onlyh add if not last burst
        if (burst < numCpuBursts - 1) {
            int ioTime = static_cast<int>(ceil(next_exp(lambda, upperBound)));

            // io-bound mean *8
            if (isIoBound) {
                ioTime *= 8;
            }

            p.addIoBurst(ioTime);
        }
    }

    return p;
}

int main(int argc, char* argv[]) {
    /*
    info stuffs for args

    argv[0] - the name of the program
    argv[1] - the first argument (the number of processes to simulate)
    argv[2] - the second argument the number that are CPU bound
    argv[3] - the third arg is the seed for the rng
    argv[4] - the fourth arg is the lambda for the exp dist
    argv[5] - the fifth arg is the upper bound for valid random numbers for exp distribution
    argv[6] - the sixth arg is the time in milliseconds it takes to perform a context switch.
    argv[7] - the seventh arg is for the SJF and SRT algorithms, since we do not know the actual CPU burst times beforehand, we will rely on estimates calculated via exponential averaging. This command-line argument is the constant α
    argv[8] - the eighth arg is  For the RR algorithm, define the time slice value, tslice, measured in milliseconds. Require tslice to be a positive integer
    */

    // need exactly 5 user args + the program name so err if not 
    if (argc != 9) {
        cerr<<"ERROR: wrong number of arguments"<<endl;
        return 1;
    }

    int n;
    int cpuBoundCount;
    int seed;
    double lambda;
    int upperBound;
    int tcs;
    double alpha;
    int tslice;

    // try to get vals from argv and make sure  valid OR ELSE EERORORRORRORROR
    try{
        n = stoi(argv[1]);
        cpuBoundCount = stoi(argv[2]);
        seed = stoi(argv[3]);
        lambda = stod(argv[4]);
        upperBound = stoi(argv[5]);
        tcs = stoi(argv[6]);
        alpha = stod(argv[7]);
        tslice = stoi(argv[8]);
    }catch (const exception&){
        cerr<<"ERROR: invalid argument type"<<endl;
        return 1;
    }

    //check the ranges 
    if(n < 1 || n > 260){
        cerr<<"ERROR: n must be between 1 and 260"<<endl;
        return 1;
    }

    if(cpuBoundCount < 0 || cpuBoundCount > n){
        cerr<<"ERROR: CPU-bound count must be between 0 and n"<<endl;
        return 1;
    }

    if(lambda <= 0.0){
        cerr<<"ERROR: lambda must be > 0"<<endl;
        return 1;
    }

    if(upperBound <= 0){
        cerr<<"ERROR: upper bound must be > 0"<<endl;
        return 1;
    }

    if(tcs <= 0 || tcs % 2 != 0){
        cerr<<"ERROR: tcs must be a positive even integer"<<endl;
        return 1;
    }

    if(alpha != -1.0 && (alpha < 0.0 || alpha > 1.0)){
        cerr<<"ERROR: alpha must be -1 or a floating-point value in [0,1]"<<endl;
        return 1;
    }

    if(tslice <= 0){
        cerr<<"ERROR: tslice must be a positive integer"<<endl;
        return 1;
    }

    //seed the normal rand() with seed
    srand48(seed);

    //print the top summary lines
    cout<<"<<<-- process set (n="<<n<<") with "<<cpuBoundCount<<" CPU-bound "<<(cpuBoundCount == 1 ? "process" : "processes")<<endl;
    cout<<"<<<-- seed="<<seed<<"; lambda="<<fixed<<setprecision(6)<<lambda<<"; upper bound="<<upperBound<<endl;
    cout<<"<<<-- t_cs="<<tcs<<"ms; alpha=";
    if (alpha == -1.0) {
        cout<<"<n/a>";
    } else {
        cout<<defaultfloat<<alpha;
    }
    cout<<"; t_slice="<<tslice<<"ms"<<endl;

    // make all the processese
    vector<Process> processes;
    

    // first n-cpuBoundCount are io-bound other are cpu-bound
    for(int i = 0;i<n;i++){
        bool isIoBound;

        if(i < (n-cpuBoundCount)){
            isIoBound = true;
        }else{
            isIoBound = false;
        }

        Process p = generateProcess(i, isIoBound, lambda, upperBound);
        processes.push_back(p);
    }

    //print all the processes stuffs
    for(size_t i = 0; i<processes.size(); i++){
        printProcess(processes[i]);
    }
    
    cout<<"<<< PROJECT SIMULATIONS"<<endl;

    Simulator sim(processes, tcs, tslice);
    sim.runSim(simulator::RR);

    writeSimout(processes, cpuBoundCount, sim.getRRStats());

    return 0;
}
