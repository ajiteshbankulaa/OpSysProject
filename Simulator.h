#include "Process.h"

namespace simulator {
    enum ALGORITHM{
        FCFS,
        SJF,
        SRT,
        RR,
        Count
    };
}

class Simulator {
    public:
        Simulator(std::vector<Process>* processes) {
            this->processes = *processes;
        }
        

        void runSim(simulator::ALGORITHM algo) {
            switch(algo) {
                case simulator::FCFS: runFCFS(); break;
                case simulator::SJF: runSJF(); break;
                case simulator::SRT: runSRT(); break;
                case simulator::RR: runRR(); break;
            };
        }
        
    private:
        std::vector<Process> processes;
        void runFCFS();
        void runSJF();
        void runSRT();
        void runRR();

};  