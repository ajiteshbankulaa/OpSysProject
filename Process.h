#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>


using namespace std;


//process class has data and shit for the proceess
class Process {
    private:
        string id;
        bool ioBound;
        int arrivalTime;
        vector<int> cpuBursts;
        vector<int> ioBursts;

    public:
        Process(const std::string& pid, bool isIoBound){
            id = pid;
            ioBound = isIoBound;
            arrivalTime = 0;
        }

        string getId() const{
            return id;
        }

        bool isIoBound() const{
            return ioBound;
        }

        int getArrivalTime() const{
            return arrivalTime;
        }

        vector<int>& getCpuBursts() {
            return cpuBursts;
        }

        const vector<int>& getCpuBursts() const {
            return cpuBursts;
        }

        vector<int>& getIoBursts() {
            return ioBursts;
        }

        const vector<int>& getIoBursts() const {
            return ioBursts;
        }

        void setArrivalTime(int t) {
            arrivalTime = t;
        }

        void addCpuBurst(int t) {
            cpuBursts.push_back(t);
        }

        void addIoBurst(int t) {
            ioBursts.push_back(t);
        }

        bool operator<(const Process& other) const {
            return arrivalTime < other.arrivalTime;
        }

};

#endif
