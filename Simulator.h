#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <algorithm>
#include <iostream>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "Process.h"

namespace simulator {
    enum ALGORITHM{
        FCFS,
        SJF,
        SRT,
        RR,
        Count
    };

    struct AlgorithmStats {
        double cpuUtilization = 0.0;
        double cpuBoundAverageWait = 0.0;
        double ioBoundAverageWait = 0.0;
        double overallAverageWait = 0.0;
        double cpuBoundAverageTurnaround = 0.0;
        double ioBoundAverageTurnaround = 0.0;
        double overallAverageTurnaround = 0.0;
        int cpuBoundContextSwitches = 0;
        int ioBoundContextSwitches = 0;
        int overallContextSwitches = 0;
        int cpuBoundPreemptions = 0;
        int ioBoundPreemptions = 0;
        int overallPreemptions = 0;
        double cpuBoundOneSlicePercent = 0.0;
        double ioBoundOneSlicePercent = 0.0;
        double overallOneSlicePercent = 0.0;
    };
}

class Simulator {
    public:
        Simulator(const std::vector<Process>& processes, int tcs, int tslice) {
            this->processes = processes;
            this->tcs = tcs;
            this->tslice = tslice;
        }

        void runSim(simulator::ALGORITHM algo) {
            switch(algo) {
                case simulator::RR: runRR(); break;
                default: break;
            }
        }

        simulator::AlgorithmStats getRRStats() const {
            return rrStats;
        }
        
    private:
        struct RuntimeProcess {
            Process process;
            int cpuBurstIndex = 0;
            int remainingCpuTime = 0;
            int readyQueueEnterTime = 0;
            int currentBurstReadyTime = 0;
            bool arrived = false;
            bool terminated = false;

            RuntimeProcess(const Process& process) : process(process) {}
        };

        std::vector<Process> processes;
        int tcs = 0;
        int tslice = 0;
        simulator::AlgorithmStats rrStats;

        std::string formatQueue(const std::queue<int>& readyQueue, const std::vector<RuntimeProcess>& runtime) const {
            if (readyQueue.empty()) {
                return "[Q:-]";
            }

            std::queue<int> copy = readyQueue;
            std::string output = "[Q:";
            while (!copy.empty()) {
                output += " " + runtime[copy.front()].process.getId();
                copy.pop();
            }
            output += "]";

            return output;
        }

        double average(long long total, int count) const {
            if (count == 0) {
                return 0.0;
            }

            return static_cast<double>(total) / static_cast<double>(count);
        }

        double percent(int value, int total) const {
            if (total == 0) {
                return 0.0;
            }

            return (static_cast<double>(value) / static_cast<double>(total)) * 100.0;
        }

        void addReadyProcess(
            int processIndex,
            int timeMS,
            std::queue<int>& readyQueue,
            std::vector<RuntimeProcess>& runtime
        ) {
            runtime[processIndex].readyQueueEnterTime = timeMS;
            if (runtime[processIndex].remainingCpuTime == 0) {
                runtime[processIndex].currentBurstReadyTime = timeMS;
            }
            readyQueue.push(processIndex);
        }

        void runRR() {
            std::vector<RuntimeProcess> runtime;
            for (const Process& process : processes) {
                runtime.push_back(RuntimeProcess(process));
            }

            std::queue<int> readyQueue;
            std::vector<std::pair<int, int> > ioCompletions;

            int timeMS = 0;
            int terminatedCount = 0;
            int currentProcess = -1;
            int nextProcess = -1;
            int cpuStartTime = -1;
            int contextSwitchCompleteTime = -1;
            int cpuBusyTime = 0;
            int lastEventTime = 0;
            int lastRunningProcess = -1;

            long long cpuBoundWaitTotal = 0;
            long long ioBoundWaitTotal = 0;
            long long cpuBoundTurnaroundTotal = 0;
            long long ioBoundTurnaroundTotal = 0;
            int cpuBoundBurstCount = 0;
            int ioBoundBurstCount = 0;
            int cpuBoundOneSliceCount = 0;
            int ioBoundOneSliceCount = 0;

            std::cout<<"time 0ms: Simulator started for RR "<<formatQueue(readyQueue, runtime)<<std::endl;

            do {
                for (std::size_t i = 0; i < runtime.size(); i++) {
                    if (!runtime[i].arrived && runtime[i].process.getArrivalTime() == timeMS) {
                        runtime[i].arrived = true;
                        addReadyProcess(static_cast<int>(i), timeMS, readyQueue, runtime);
                        std::cout<<"time "<<timeMS<<"ms: Process "<<runtime[i].process.getId()<<" arrived; added to ready queue "<<formatQueue(readyQueue, runtime)<<std::endl;
                    }
                }

                for (std::size_t i = 0; i < ioCompletions.size(); ) {
                    if (ioCompletions[i].first == timeMS) {
                        int processIndex = ioCompletions[i].second;
                        addReadyProcess(processIndex, timeMS, readyQueue, runtime);
                        std::cout<<"time "<<timeMS<<"ms: Process "<<runtime[processIndex].process.getId()<<" completed I/O; added to ready queue "<<formatQueue(readyQueue, runtime)<<std::endl;
                        ioCompletions.erase(ioCompletions.begin() + i);
                    } else {
                        i++;
                    }
                }

                if (nextProcess == -1 && currentProcess == -1 && !readyQueue.empty()) {
                    nextProcess = readyQueue.front();
                    readyQueue.pop();
                    contextSwitchCompleteTime = timeMS + (lastRunningProcess == -1 ? tcs / 2 : tcs);
                }

                if (nextProcess != -1 && contextSwitchCompleteTime == timeMS) {
                    currentProcess = nextProcess;
                    nextProcess = -1;
                    cpuStartTime = timeMS;
                    RuntimeProcess& running = runtime[currentProcess];
                    const std::vector<int>& cpuBursts = running.process.getCpuBursts();
                    int fullBurstTime = cpuBursts[running.cpuBurstIndex];

                    if (running.remainingCpuTime == 0) {
                        running.remainingCpuTime = fullBurstTime;
                    }

                    int waitTime = timeMS - running.readyQueueEnterTime;
                    if (running.process.isIoBound()) {
                        ioBoundWaitTotal += waitTime;
                    } else {
                        cpuBoundWaitTotal += waitTime;
                    }

                    if (running.remainingCpuTime == fullBurstTime) {
                        std::cout<<"time "<<timeMS<<"ms: Process "<<running.process.getId()<<" started using the CPU for "<<fullBurstTime<<"ms burst "<<formatQueue(readyQueue, runtime)<<std::endl;
                    } else {
                        std::cout<<"time "<<timeMS<<"ms: Process "<<running.process.getId()<<" started using the CPU for remaining "<<running.remainingCpuTime<<"ms of "<<fullBurstTime<<"ms burst "<<formatQueue(readyQueue, runtime)<<std::endl;
                    }
                }

                if (currentProcess != -1) {
                    RuntimeProcess& running = runtime[currentProcess];
                    int elapsed = timeMS - cpuStartTime;

                    if (running.remainingCpuTime == elapsed) {
                        const std::vector<int>& cpuBursts = running.process.getCpuBursts();
                        const std::vector<int>& ioBursts = running.process.getIoBursts();
                        int fullBurstTime = cpuBursts[running.cpuBurstIndex];
                        int turnaroundTime = timeMS + tcs / 2 - running.currentBurstReadyTime;

                        cpuBusyTime += elapsed;
                        running.remainingCpuTime = 0;

                        if (running.process.isIoBound()) {
                            ioBoundBurstCount++;
                            ioBoundTurnaroundTotal += turnaroundTime;
                            if (fullBurstTime <= tslice) {
                                ioBoundOneSliceCount++;
                            }
                        } else {
                            cpuBoundBurstCount++;
                            cpuBoundTurnaroundTotal += turnaroundTime;
                            if (fullBurstTime <= tslice) {
                                cpuBoundOneSliceCount++;
                            }
                        }

                        int burstsLeft = static_cast<int>(cpuBursts.size()) - running.cpuBurstIndex - 1;
                        if (burstsLeft == 0) {
                            running.terminated = true;
                            terminatedCount++;
                            lastEventTime = timeMS + tcs / 2;
                            std::cout<<"time "<<timeMS<<"ms: Process "<<running.process.getId()<<" terminated "<<formatQueue(readyQueue, runtime)<<std::endl;
                        } else {
                            std::cout<<"time "<<timeMS<<"ms: Process "<<running.process.getId()<<" completed a CPU burst; "<<burstsLeft<<" "<<(burstsLeft == 1 ? "burst" : "bursts")<<" to go "<<formatQueue(readyQueue, runtime)<<std::endl;

                            int ioCompletionTime = timeMS + ioBursts[running.cpuBurstIndex] + tcs / 2;
                            std::cout<<"time "<<timeMS<<"ms: Process "<<running.process.getId()<<" switching out of CPU; blocking on I/O until time "<<ioCompletionTime<<"ms "<<formatQueue(readyQueue, runtime)<<std::endl;
                            ioCompletions.push_back(std::make_pair(ioCompletionTime, currentProcess));
                            std::sort(ioCompletions.begin(), ioCompletions.end());
                            running.cpuBurstIndex++;
                        }

                        lastRunningProcess = currentProcess;
                        currentProcess = -1;
                        cpuStartTime = -1;
                    } else if (elapsed > 0 && elapsed % tslice == 0) {
                        running.remainingCpuTime -= tslice;
                        cpuBusyTime += tslice;

                        if (readyQueue.empty()) {
                            std::cout<<"time "<<timeMS<<"ms: Time slice expired; no preemption because ready queue is empty "<<formatQueue(readyQueue, runtime)<<std::endl;
                            cpuStartTime = timeMS;
                        } else {
                            if (running.process.isIoBound()) {
                                rrStats.ioBoundPreemptions++;
                            } else {
                                rrStats.cpuBoundPreemptions++;
                            }

                            std::cout<<"time "<<timeMS<<"ms: Time slice expired; preempting process "<<running.process.getId()<<" with "<<running.remainingCpuTime<<"ms remaining "<<formatQueue(readyQueue, runtime)<<std::endl;
                            addReadyProcess(currentProcess, timeMS, readyQueue, runtime);
                            lastRunningProcess = currentProcess;
                            currentProcess = -1;
                            cpuStartTime = -1;
                        }
                    }
                }

                if (nextProcess == -1 && currentProcess == -1 && !readyQueue.empty()) {
                    nextProcess = readyQueue.front();
                    readyQueue.pop();
                    contextSwitchCompleteTime = timeMS + (lastRunningProcess == -1 ? tcs / 2 : tcs);
                }

                if (terminatedCount == static_cast<int>(runtime.size())) {
                    break;
                }
            } while(1 | timeMS++);

            lastEventTime = std::max(lastEventTime, timeMS + tcs / 2);
            rrStats.cpuBoundContextSwitches = cpuBoundBurstCount + rrStats.cpuBoundPreemptions;
            rrStats.ioBoundContextSwitches = ioBoundBurstCount + rrStats.ioBoundPreemptions;
            rrStats.overallContextSwitches = rrStats.cpuBoundContextSwitches + rrStats.ioBoundContextSwitches;
            rrStats.overallPreemptions = rrStats.cpuBoundPreemptions + rrStats.ioBoundPreemptions;
            rrStats.cpuUtilization = percent(cpuBusyTime, lastEventTime);
            rrStats.cpuBoundAverageWait = average(cpuBoundWaitTotal, cpuBoundBurstCount);
            rrStats.ioBoundAverageWait = average(ioBoundWaitTotal, ioBoundBurstCount);
            rrStats.overallAverageWait = average(cpuBoundWaitTotal + ioBoundWaitTotal, cpuBoundBurstCount + ioBoundBurstCount);
            rrStats.cpuBoundAverageTurnaround = average(cpuBoundTurnaroundTotal, cpuBoundBurstCount);
            rrStats.ioBoundAverageTurnaround = average(ioBoundTurnaroundTotal, ioBoundBurstCount);
            rrStats.overallAverageTurnaround = average(cpuBoundTurnaroundTotal + ioBoundTurnaroundTotal, cpuBoundBurstCount + ioBoundBurstCount);
            rrStats.cpuBoundOneSlicePercent = percent(cpuBoundOneSliceCount, cpuBoundBurstCount);
            rrStats.ioBoundOneSlicePercent = percent(ioBoundOneSliceCount, ioBoundBurstCount);
            rrStats.overallOneSlicePercent = percent(cpuBoundOneSliceCount + ioBoundOneSliceCount, cpuBoundBurstCount + ioBoundBurstCount);

            std::cout<<"time "<<lastEventTime<<"ms: Simulator ended for RR "<<formatQueue(readyQueue, runtime)<<std::endl;
        }
};

#endif
