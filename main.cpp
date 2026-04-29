#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <iomanip>
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

//prnt singluar instance of a process in the format
void printProcess(Process& p) {

    vector<int>& cpu = p.getCpuBursts();
    vector<int>& io = p.getIoBursts();

    if (p.isIoBound()){
        cout<<"I/O-bound process "<<p.getId()<<": arrival time "<<p.getArrivalTime()<<"ms; "<<cpu.size()<<" "<<(cpu.size() == 1 ? "CPU burst" : "CPU bursts")<<":"<<endl;
    }else{
        cout<<"CPU-bound process "<<p.getId()<<": arrival time "<<p.getArrivalTime()<<"ms; "<<cpu.size()<<" "<<(cpu.size() == 1 ? "CPU burst" : "CPU bursts")<<":"<<endl;
    }

    for(size_t i = 0; i < cpu.size(); i++){
        if(i < io.size()){
            cout<<"==> CPU burst "<<cpu[i]<<"ms ==> I/O burst "<<io[i]<<"ms"<<endl;
        }else{
            cout<<"==> CPU burst "<<cpu[i]<<"ms"<<endl;
        }
    }

    cout<<endl;
}

void printProcess2(Process& p) {

    vector<int>& cpu = p.getCpuBursts();
    vector<int>& io = p.getIoBursts();

    if (p.isIoBound()){
        cout<<"I/O-bound process "<<p.getId()<<": arrival time "<<p.getArrivalTime()<<"ms; "<<cpu.size()<<" "<<(cpu.size() == 1 ? "CPU burst" : "CPU bursts")<<":"<<endl;
    }else{
        cout<<"CPU-bound process "<<p.getId()<<": arrival time "<<p.getArrivalTime()<<"ms; "<<cpu.size()<<" "<<(cpu.size() == 1 ? "CPU burst" : "CPU bursts")<<":"<<endl;
    }

    for(size_t i = 0; i < cpu.size(); i++){
        if(i < io.size()){
            cout<<"==> CPU burst "<<cpu[i]<<"ms ==> I/O burst "<<io[i]<<"ms"<<endl;
        }else{
            cout<<"==> CPU burst "<<cpu[i]<<"ms"<<endl;
        }
    }

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
    cout<<"<<< -- process set (n="<<n<<") with "<<cpuBoundCount<<" CPU-bound "<<(cpuBoundCount == 1 ? "process" : "processes")<<endl;
    cout<<"<<< -- seed="<<seed<<"; lambda="<<fixed<<setprecision(6)<<lambda<<"; upper bound="<<upperBound<<endl;
    cout<<endl;

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
        if(i != processes.size() - 1){
            printProcess(processes[i]); 
        }else{
            printProcess2(processes[i]);
        }
    }
    
    Simulator sim(&processes);

    for (int i = 0; i < simulator::Count; i++) {
        sim.runSim(static_cast<simulator::ALGORITHM>(i));
    }

    return 0;
}
