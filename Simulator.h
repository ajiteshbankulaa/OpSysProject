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
        Simulator(const std::vector<Process>& processes, int tcs, int tslice, double alpha, double lambda) {
            this->processes = processes;
            this->tcs = tcs;
            this->tslice = tslice;
            this->alpha = alpha;
            this->lambda = lambda;
        }

        void runSim(simulator::ALGORITHM algo) {
            switch(algo) {
                case simulator::SRT: runSRT(); break;
                case simulator::RR:  runRR();  break;
                case simulator::FCFS: runFCFS(); break;
                case simulator::SJF: runFCFS(true); break;
                default: break;
            }

            cout<<endl;
        }

        simulator::AlgorithmStats getRRStats() const {
            return rrStats;
        }
        simulator::AlgorithmStats getSRTStats() const {
            return srtStats;
        }
        simulator::AlgorithmStats getFCFSStats() const {
            return fcfsStats;
        }
        simulator::AlgorithmStats getSJFStats() const {
            return sjfStats;
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
            int tau = 0;
            int remainingPredicted = 0;

            RuntimeProcess(const Process& process) : process(process) {}
        };

        std::vector<Process> processes;
        int tcs = 0;
        int tslice = 0;
        simulator::AlgorithmStats fcfsStats;
        simulator::AlgorithmStats sjfStats;
        simulator::AlgorithmStats rrStats;
        double alpha = 0.0;
        double lambda = 0.0;
        simulator::AlgorithmStats srtStats;

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
        
        std::string formatQueueVec(const std::vector<int>& readyQueue, const std::vector<RuntimeProcess>& runtime) const {
            if (readyQueue.empty()) {
                return "[Q: -]";
            }
            std::string output = "[Q:";
            for (int idx : readyQueue) {
                output += " " + runtime[idx].process.getId();
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



        void srtEnqueue(
            int processIndex,
            int timeMS,
            std::vector<int>& readyQueue,
            std::vector<RuntimeProcess>& runtime
        ) {
            runtime[processIndex].readyQueueEnterTime = timeMS;

            //only reset burst tracking fields if this is a fresh burst, not a preempted return
            if (runtime[processIndex].remainingCpuTime == 0) {
                runtime[processIndex].currentBurstReadyTime = timeMS;
            }

            //find correct sorted position
            auto it = readyQueue.begin();
            while (it != readyQueue.end()) {
                int existingPred = runtime[*it].remainingPredicted;
                int newPred = runtime[processIndex].remainingPredicted;
                if (newPred < existingPred) break;
                if (newPred == existingPred &&
                    runtime[processIndex].process.getId() < runtime[*it].process.getId()) break;
                ++it;
            }

            readyQueue.insert(it, processIndex);
        }

        void runFCFS(bool is_sjf = false) {
            std::vector<RuntimeProcess> runtime;
            for (const Process& p : processes) {
                runtime.push_back(RuntimeProcess(p));
            }

            std::vector<int> readyQueue;
            std::vector<std::pair<int,int>> ioCompletions;

            simulator::AlgorithmStats & stats = is_sjf ? sjfStats : fcfsStats;

            long long timeMS = 0;
            unsigned int terminatedCount = 0;
            int currentProcess = -1;
            int nextProcess = -1;
            int cpuStartTime = -1;
            int contextSwitchCompleteTime = -1;
            int lastRunningProcess = -1;

            long long cpuBusyTime = 0;

            long long cpuBoundWaitTotal = 0, ioBoundWaitTotal = 0;
            long long cpuBoundTurnaroundTotal = 0, ioBoundTurnaroundTotal = 0;

            int cpuBoundBurstCount = 0, ioBoundBurstCount = 0;
            int cpuBoundOneSliceCount = 0, ioBoundOneSliceCount = 0;

            std::string algoName = is_sjf ? "SJF" : "FCFS";

            std::cout << "time 0ms: Simulator started for " << algoName
                    << " " << formatQueueVec(readyQueue, runtime) << std::endl;

            do {
                if (currentProcess != -1) {
                    RuntimeProcess& running = runtime[currentProcess];
                    int elapsed = timeMS - cpuStartTime;

                    if (running.remainingCpuTime == elapsed) {
                        const auto& cpuBursts = running.process.getCpuBursts();
                        const auto& ioBursts  = running.process.getIoBursts();

                        // CPU utilization
                        cpuBusyTime += elapsed;

                        // Turnaround time
                        long long turnaround =
                            timeMS - running.readyQueueEnterTime;

                        if (running.process.isIoBound()) {
                            ioBoundTurnaroundTotal += turnaround;
                            ioBoundBurstCount++;
                            ioBoundOneSliceCount++;
                        } else {
                            cpuBoundTurnaroundTotal += turnaround;
                            cpuBoundBurstCount++;
                            cpuBoundOneSliceCount++;
                        }

                        running.remainingCpuTime = 0;
                        int burstsLeft = cpuBursts.size() - running.cpuBurstIndex - 1;

                        if (burstsLeft == 0) {
                            running.terminated = true;
                            terminatedCount++;

                            std::cout << "time " << timeMS << "ms: Process "
                                    << running.process.getId() << " terminated "
                                    << formatQueueVec(readyQueue, runtime) << std::endl;
                        } else {
                            std::cout << "time " << timeMS << "ms: Process "
                                    << running.process.getId() << " completed a CPU burst; "
                                    << burstsLeft << (burstsLeft == 1 ? " burst" : " bursts")
                                    << " to go " << formatQueueVec(readyQueue, runtime) << std::endl;

                            int ioCompletionTime =
                                timeMS + ioBursts[running.cpuBurstIndex] + tcs / 2;

                            std::cout << "time " << timeMS << "ms: Process "
                                    << running.process.getId()
                                    << " switching out of CPU; blocking on I/O until time "
                                    << ioCompletionTime << "ms "
                                    << formatQueueVec(readyQueue, runtime) << std::endl;

                            ioCompletions.push_back({ioCompletionTime, currentProcess});
                            running.cpuBurstIndex++;
                        }

                        lastRunningProcess = currentProcess;
                        currentProcess = -1;
                        cpuStartTime = -1;
                    }
                }

                // ======================
                // Context switch completion
                // ======================
                if (nextProcess != -1 && contextSwitchCompleteTime == timeMS) {
                    currentProcess = nextProcess;
                    nextProcess = -1;

                    RuntimeProcess& running = runtime[currentProcess];
                    const auto& cpuBursts = running.process.getCpuBursts();

                    if (running.remainingCpuTime == 0) {
                        running.remainingCpuTime =
                            cpuBursts[running.cpuBurstIndex];
                    }

                    // Wait time
                    long long wait =
                        timeMS - running.readyQueueEnterTime;

                    if (running.process.isIoBound()) {
                        ioBoundWaitTotal += wait;
                    } else {
                        cpuBoundWaitTotal += wait;

                    }

                    cpuStartTime = timeMS;

                    std::cout << "time " << timeMS << "ms: Process "
                            << running.process.getId()
                            << " started using the CPU for "
                            << running.remainingCpuTime << "ms burst "
                            << formatQueueVec(readyQueue, runtime) << std::endl;
                }

                for (size_t i = 0; i < ioCompletions.size();) {
                    if (ioCompletions[i].first == timeMS) {
                        int idx = ioCompletions[i].second;
                        ioCompletions.erase(ioCompletions.begin() + i);

                        readyQueue.push_back(idx);
                        runtime[idx].readyQueueEnterTime = timeMS;

                        std::cout << "time " << timeMS << "ms: Process "
                                << runtime[idx].process.getId()
                                << " completed I/O; added to ready queue "
                                << formatQueueVec(readyQueue, runtime) << std::endl;
                    } else {
                        i++;
                    }
                }

                for (size_t i = 0; i < runtime.size(); i++) {
                    if (!runtime[i].arrived &&
                        runtime[i].process.getArrivalTime() == timeMS) {

                        runtime[i].arrived = true;
                        readyQueue.push_back(i);
                        runtime[i].readyQueueEnterTime = timeMS;

                        std::cout << "time " << timeMS << "ms: Process "
                                << runtime[i].process.getId()
                                << " arrived; added to ready queue "
                                << formatQueueVec(readyQueue, runtime) << std::endl;
                    }
                }

                if (currentProcess == -1 && nextProcess == -1 && !readyQueue.empty()) {
                    // Sort ready queue
                    if (is_sjf) {
                        std::sort(readyQueue.begin(), readyQueue.end(),
                        [runtime](int a, int b) {
                            return runtime[a].process.getCpuBursts()[runtime[a].cpuBurstIndex] < runtime[b].process.getCpuBursts()[runtime[b].cpuBurstIndex];
                        }                       
                    ); // normal sort
                    } else {
                        std::sort(readyQueue.begin(), readyQueue.end()); // normal sort
                    }

                    nextProcess = readyQueue.front();
                    readyQueue.erase(readyQueue.begin());

                    contextSwitchCompleteTime =
                        timeMS + (lastRunningProcess == -1 ? tcs / 2 : tcs);
                }

                if (terminatedCount == runtime.size()) break;

            } while (++timeMS);

            stats.overallPreemptions = 0;

            stats.cpuBoundContextSwitches = cpuBoundBurstCount;
            stats.ioBoundContextSwitches = ioBoundBurstCount;
            stats.overallContextSwitches =
                cpuBoundBurstCount + ioBoundBurstCount;

            stats.cpuUtilization =
                percent(cpuBusyTime, timeMS);

            stats.cpuBoundAverageWait =
                average(cpuBoundWaitTotal, cpuBoundBurstCount);
            stats.ioBoundAverageWait =
                average(ioBoundWaitTotal, ioBoundBurstCount);
            stats.overallAverageWait =
                average(cpuBoundWaitTotal + ioBoundWaitTotal,
                        cpuBoundBurstCount + ioBoundBurstCount);

            stats.cpuBoundAverageTurnaround =
                average(cpuBoundTurnaroundTotal, cpuBoundBurstCount);
            stats.ioBoundAverageTurnaround =
                average(ioBoundTurnaroundTotal, ioBoundBurstCount);
            stats.overallAverageTurnaround =
                average(cpuBoundTurnaroundTotal + ioBoundTurnaroundTotal,
                        cpuBoundBurstCount + ioBoundBurstCount);

            stats.cpuBoundOneSlicePercent =
                percent(cpuBoundOneSliceCount, cpuBoundBurstCount);
            stats.ioBoundOneSlicePercent =
                percent(ioBoundOneSliceCount, ioBoundBurstCount);
            stats.overallOneSlicePercent =
                percent(cpuBoundOneSliceCount + ioBoundOneSliceCount,
                        cpuBoundBurstCount + ioBoundBurstCount);

            std::cout << "time " << timeMS + tcs / 2
                    << "ms: Simulator ended for " << algoName
                    << " " << formatQueueVec(readyQueue, runtime) << std::endl;
        }  
      void runSRT() {
            std::vector<RuntimeProcess> runtime;
            for (const Process& p : processes) {
                runtime.push_back(RuntimeProcess(p));
            }

            //initialize tau for every process
            int initTau = static_cast<int>(ceil(1.0 / lambda));
            for (RuntimeProcess& rp : runtime) {
                rp.tau = initTau;
            }

            std::vector<int> readyQueue;
            std::vector<std::pair<int,int>> ioCompletions;

            int timeMS = 0;
            int terminatedCount = 0;
            int currentProcess = -1;
            int nextProcess = -1;
            int cpuStartTime = -1;
            int contextSwitchCompleteTime = -1;
            int cpuBusyTime = 0;
            int lastEventTime = 0;
            int lastRunningProcess = -1;

            long long cpuBoundWaitTotal = 0, ioBoundWaitTotal = 0;
            long long cpuBoundTurnaroundTotal = 0, ioBoundTurnaroundTotal = 0;
            int cpuBoundBurstCount = 0, ioBoundBurstCount = 0;

            bool isOpt = (alpha == -1.0);
            std::string algoName = isOpt ? "SRT-OPT" : "SRT";

            std::cout << "time 0ms: Simulator started for " << algoName
                    << " " << formatQueueVec(readyQueue, runtime) << std::endl;

            do {
                if (currentProcess != -1) {
                    RuntimeProcess& running = runtime[currentProcess];
                    int elapsed = timeMS - cpuStartTime;

                    if (running.remainingCpuTime == elapsed) {
                        const std::vector<int>& cpuBursts = running.process.getCpuBursts();
                        const std::vector<int>& ioBursts   = running.process.getIoBursts();
                        int actualBurst    = cpuBursts[running.cpuBurstIndex];
                        int turnaroundTime = timeMS + tcs / 2 - running.currentBurstReadyTime;

                        cpuBusyTime += elapsed;
                        running.remainingCpuTime  = 0;
                        running.remainingPredicted = 0;

                        if (running.process.isIoBound()) {
                            ioBoundBurstCount++;
                            ioBoundTurnaroundTotal += turnaroundTime;
                        } else {
                            cpuBoundBurstCount++;
                            cpuBoundTurnaroundTotal += turnaroundTime;
                        }

                        int burstsLeft = static_cast<int>(cpuBursts.size()) - running.cpuBurstIndex - 1;

                        if (burstsLeft == 0) {
                            running.terminated = true;
                            terminatedCount++;
                            lastEventTime = timeMS + tcs / 2;
                            std::cout << "time " << timeMS << "ms: Process "
                                    << running.process.getId() << " terminated "
                                    << formatQueueVec(readyQueue, runtime) << std::endl;
                        } else {
                            std::cout << "time " << timeMS << "ms: Process "
                                    << running.process.getId() << " completed a CPU burst; "
                                    << burstsLeft << (burstsLeft == 1 ? " burst" : " bursts")
                                    << " to go " << formatQueueVec(readyQueue, runtime) << std::endl;

                            //recalculate tau
                            if (!isOpt) {
                                int newTau = static_cast<int>(ceil(alpha * actualBurst + (1.0 - alpha) * running.tau));
                                running.tau = newTau;
                                std::cout << "time " << timeMS << "ms: Recalculated tau for process "
                                        << running.process.getId() << " to " << newTau << "ms "
                                        << formatQueueVec(readyQueue, runtime) << std::endl;
                            }

                            int ioCompletionTime = timeMS + ioBursts[running.cpuBurstIndex] + tcs / 2;
                            std::cout << "time " << timeMS << "ms: Process "
                                    << running.process.getId()
                                    << " switching out of CPU; blocking on I/O until time "
                                    << ioCompletionTime << "ms "
                                    << formatQueueVec(readyQueue, runtime) << std::endl;

                            ioCompletions.push_back({ioCompletionTime, currentProcess});
                            std::sort(ioCompletions.begin(), ioCompletions.end());
                            running.cpuBurstIndex++;
                        }

                        lastRunningProcess = currentProcess;
                        currentProcess = -1;
                        cpuStartTime = -1;
                    }
                }

                //process starts
                if (nextProcess != -1 && contextSwitchCompleteTime == timeMS) {
                    currentProcess = nextProcess;
                    nextProcess = -1;
                    cpuStartTime = timeMS;
                    RuntimeProcess& running = runtime[currentProcess];
                    const std::vector<int>& cpuBursts = running.process.getCpuBursts();
                    int fullBurst = cpuBursts[running.cpuBurstIndex];

                    if (running.remainingCpuTime == 0) {
                        running.remainingCpuTime = fullBurst;
                    }

                    int waitTime = timeMS - running.readyQueueEnterTime;
                    if (running.process.isIoBound()) ioBoundWaitTotal += waitTime;
                    else cpuBoundWaitTotal += waitTime;

                    bool isFull = (running.remainingCpuTime == fullBurst);

                    if (!isOpt) {
                        if (isFull)
                            std::cout << "time " << timeMS << "ms: Process " << running.process.getId()
                                    << " (tau " << running.tau << "ms) started using the CPU for "
                                    << fullBurst << "ms burst "
                                    << formatQueueVec(readyQueue, runtime) << std::endl;
                        else
                            std::cout << "time " << timeMS << "ms: Process " << running.process.getId()
                                    << " (tau " << running.tau << "ms) started using the CPU for remaining "
                                    << running.remainingCpuTime << "ms of " << fullBurst << "ms burst "
                                    << formatQueueVec(readyQueue, runtime) << std::endl;
                    } else {
                        if (isFull)
                            std::cout << "time " << timeMS << "ms: Process " << running.process.getId()
                                    << " started using the CPU for " << fullBurst << "ms burst "
                                    << formatQueueVec(readyQueue, runtime) << std::endl;
                        else
                            std::cout << "time " << timeMS << "ms: Process " << running.process.getId()
                                    << " started using the CPU for remaining "
                                    << running.remainingCpuTime << "ms of " << fullBurst << "ms burst "
                                    << formatQueueVec(readyQueue, runtime) << std::endl;
                    }
                }

                //I/O
                {
                    std::vector<int> completingNow;
                    for (std::size_t i = 0; i < ioCompletions.size(); ) {
                        if (ioCompletions[i].first == timeMS) {
                            completingNow.push_back(ioCompletions[i].second);
                            ioCompletions.erase(ioCompletions.begin() + i);
                        } else {
                            i++;
                        }
                    }

                    //tie break
                    std::sort(completingNow.begin(), completingNow.end(), [&](int a, int b) {
                        return runtime[a].process.getId() < runtime[b].process.getId();
                    });

                    for (int idx : completingNow) {
                        RuntimeProcess& rp = runtime[idx];
                        const std::vector<int>& cpuBursts = rp.process.getCpuBursts();

                        //set remainingPredicted
                        rp.remainingPredicted = isOpt ? cpuBursts[rp.cpuBurstIndex] : rp.tau;

                        bool didPreempt = false;

                        //check preemption
                        if (currentProcess != -1 && nextProcess == -1) {
                            RuntimeProcess& running = runtime[currentProcess];
                            int elapsed = timeMS - cpuStartTime;
                            int runRemPred = running.remainingPredicted - elapsed;

                            if (rp.remainingPredicted < runRemPred ||
                            (rp.remainingPredicted == runRemPred &&
                                rp.process.getId() < running.process.getId())) {

                                cpuBusyTime += elapsed;
                                running.remainingCpuTime  -= elapsed;
                                running.remainingPredicted = runRemPred;

                                if (running.process.isIoBound()) srtStats.ioBoundPreemptions++;
                                else srtStats.cpuBoundPreemptions++;

                                //print
                                if (!isOpt)
                                    std::cout << "time " << timeMS << "ms: Process " << rp.process.getId()
                                            << " (tau " << rp.tau << "ms) completed I/O; preempting "
                                            << running.process.getId()
                                            << " (predicted remaining time " << runRemPred << "ms) "
                                            << formatQueueVec(readyQueue, runtime) << std::endl;
                                else
                                    std::cout << "time " << timeMS << "ms: Process " << rp.process.getId()
                                            << " completed I/O; preempting " << running.process.getId()
                                            << " (remaining time " << running.remainingCpuTime << "ms) "
                                            << formatQueueVec(readyQueue, runtime) << std::endl;

                                srtEnqueue(currentProcess, timeMS, readyQueue, runtime);
                                lastRunningProcess = currentProcess;
                                currentProcess = -1;
                                cpuStartTime = -1;

                                
                                rp.readyQueueEnterTime   = timeMS;
                                rp.currentBurstReadyTime = timeMS;
                                nextProcess = idx;
                                contextSwitchCompleteTime = timeMS + tcs;

                                didPreempt = true;
                            }
                        }

                        if (!didPreempt) {
                            srtEnqueue(idx, timeMS, readyQueue, runtime);
                            if (!isOpt)
                                std::cout << "time " << timeMS << "ms: Process " << rp.process.getId()
                                        << " (tau " << rp.tau << "ms) completed I/O; added to ready queue "
                                        << formatQueueVec(readyQueue, runtime) << std::endl;
                            else
                                std::cout << "time " << timeMS << "ms: Process " << rp.process.getId()
                                        << " completed I/O; added to ready queue "
                                        << formatQueueVec(readyQueue, runtime) << std::endl;
                        }
                    }
                }

                //new process arrival
                for (std::size_t i = 0; i < runtime.size(); i++) {
                    if (!runtime[i].arrived && runtime[i].process.getArrivalTime() == timeMS) {
                        runtime[i].arrived = true;

                        const std::vector<int>& cpuBursts = runtime[i].process.getCpuBursts();
                        runtime[i].remainingPredicted = isOpt ? cpuBursts[0] : runtime[i].tau;

                        bool didPreempt = false;

                        if (currentProcess != -1 && nextProcess == -1) {
                            RuntimeProcess& running = runtime[currentProcess];
                            int elapsed = timeMS - cpuStartTime;
                            int runRemPred = running.remainingPredicted - elapsed;

                            if (runtime[i].remainingPredicted < runRemPred ||
                            (runtime[i].remainingPredicted == runRemPred &&
                                runtime[i].process.getId() < running.process.getId())) {

                                cpuBusyTime += elapsed;
                                running.remainingCpuTime  -= elapsed;
                                running.remainingPredicted = runRemPred;

                                if (running.process.isIoBound()) srtStats.ioBoundPreemptions++;
                                else srtStats.cpuBoundPreemptions++;

                                //print
                                if (!isOpt)
                                    std::cout << "time " << timeMS << "ms: Process "
                                            << runtime[i].process.getId()
                                            << " (tau " << runtime[i].tau << "ms) arrived; preempting "
                                            << running.process.getId()
                                            << " (predicted remaining time " << runRemPred << "ms) "
                                            << formatQueueVec(readyQueue, runtime) << std::endl;
                                else
                                    std::cout << "time " << timeMS << "ms: Process "
                                            << runtime[i].process.getId()
                                            << " arrived; preempting " << running.process.getId()
                                            << " (remaining time " << running.remainingCpuTime << "ms) "
                                            << formatQueueVec(readyQueue, runtime) << std::endl;

                                srtEnqueue(currentProcess, timeMS, readyQueue, runtime);
                                lastRunningProcess = currentProcess;
                                currentProcess = -1;
                                cpuStartTime = -1;

                                runtime[i].readyQueueEnterTime   = timeMS;
                                runtime[i].currentBurstReadyTime = timeMS;
                                nextProcess = static_cast<int>(i);
                                contextSwitchCompleteTime = timeMS + tcs;

                                didPreempt = true;
                            }
                        }

                        if (!didPreempt) {
                            srtEnqueue(static_cast<int>(i), timeMS, readyQueue, runtime);
                            if (!isOpt)
                                std::cout << "time " << timeMS << "ms: Process "
                                        << runtime[i].process.getId()
                                        << " (tau " << runtime[i].tau << "ms) arrived; added to ready queue "
                                        << formatQueueVec(readyQueue, runtime) << std::endl;
                            else
                                std::cout << "time " << timeMS << "ms: Process "
                                        << runtime[i].process.getId()
                                        << " arrived; added to ready queue "
                                        << formatQueueVec(readyQueue, runtime) << std::endl;
                        }
                    }
                }

                //start next context switch if CPU free and queue not empty
                if (nextProcess == -1 && currentProcess == -1 && !readyQueue.empty()) {
                    nextProcess = readyQueue.front();
                    readyQueue.erase(readyQueue.begin());
                    contextSwitchCompleteTime = timeMS + (lastRunningProcess == -1 ? tcs / 2 : tcs);
                }

                if (terminatedCount == static_cast<int>(runtime.size())) break;

            } while (++timeMS);

            lastEventTime = std::max(lastEventTime, timeMS + tcs / 2);
            srtStats.cpuBoundContextSwitches = cpuBoundBurstCount + srtStats.cpuBoundPreemptions;
            srtStats.ioBoundContextSwitches  = ioBoundBurstCount  + srtStats.ioBoundPreemptions;
            srtStats.overallContextSwitches  = srtStats.cpuBoundContextSwitches + srtStats.ioBoundContextSwitches;
            srtStats.overallPreemptions      = srtStats.cpuBoundPreemptions + srtStats.ioBoundPreemptions;
            srtStats.cpuUtilization          = percent(cpuBusyTime, lastEventTime);
            srtStats.cpuBoundAverageWait     = average(cpuBoundWaitTotal, cpuBoundBurstCount);
            srtStats.ioBoundAverageWait      = average(ioBoundWaitTotal,  ioBoundBurstCount);
            srtStats.overallAverageWait      = average(cpuBoundWaitTotal + ioBoundWaitTotal,
                                                    cpuBoundBurstCount + ioBoundBurstCount);
            srtStats.cpuBoundAverageTurnaround = average(cpuBoundTurnaroundTotal, cpuBoundBurstCount);
            srtStats.ioBoundAverageTurnaround  = average(ioBoundTurnaroundTotal,  ioBoundBurstCount);
            srtStats.overallAverageTurnaround  = average(cpuBoundTurnaroundTotal + ioBoundTurnaroundTotal,
                                                        cpuBoundBurstCount + ioBoundBurstCount);

            std::cout << "time " << lastEventTime << "ms: Simulator ended for " << algoName
                    << " " << formatQueueVec(readyQueue, runtime) << std::endl;
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
            int switchOutCompleteTime = 0;
            const int eventPrintCutoff = 10000;

            long long cpuBoundWaitTotal = 0;
            long long ioBoundWaitTotal = 0;
            long long cpuBoundTurnaroundTotal = 0;
            long long ioBoundTurnaroundTotal = 0;
            long long cpuBoundCpuBusyTime = 0;
            long long ioBoundCpuBusyTime = 0;
            int cpuBoundBurstCount = 0;
            int ioBoundBurstCount = 0;
            int cpuBoundOneSliceCount = 0;
            int ioBoundOneSliceCount = 0;

            std::cout<<"time 0ms: Simulator started for RR "<<formatQueue(readyQueue, runtime)<<std::endl;

            do {
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
                            ioBoundCpuBusyTime += elapsed;
                            ioBoundBurstCount++;
                            ioBoundTurnaroundTotal += turnaroundTime;
                            if (fullBurstTime <= tslice) {
                                ioBoundOneSliceCount++;
                            }
                        } else {
                            cpuBoundCpuBusyTime += elapsed;
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
                            if (timeMS < eventPrintCutoff) {
                                std::cout<<"time "<<timeMS<<"ms: Process "<<running.process.getId()<<" completed a CPU burst; "<<burstsLeft<<" "<<(burstsLeft == 1 ? "burst" : "bursts")<<" to go "<<formatQueue(readyQueue, runtime)<<std::endl;
                            }

                            int ioCompletionTime = timeMS + ioBursts[running.cpuBurstIndex] + tcs / 2;
                            if (timeMS < eventPrintCutoff) {
                                std::cout<<"time "<<timeMS<<"ms: Process "<<running.process.getId()<<" switching out of CPU; blocking on I/O until time "<<ioCompletionTime<<"ms "<<formatQueue(readyQueue, runtime)<<std::endl;
                            }
                            ioCompletions.push_back(std::make_pair(ioCompletionTime, currentProcess));
                            std::sort(ioCompletions.begin(), ioCompletions.end());
                            running.cpuBurstIndex++;
                        }

                        switchOutCompleteTime = timeMS + tcs / 2;
                        currentProcess = -1;
                        cpuStartTime = -1;
                    }
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

                    if (running.remainingCpuTime == fullBurstTime) {
                        if (timeMS < eventPrintCutoff) {
                            std::cout<<"time "<<timeMS<<"ms: Process "<<running.process.getId()<<" started using the CPU for "<<fullBurstTime<<"ms burst "<<formatQueue(readyQueue, runtime)<<std::endl;
                        }
                    } else {
                        if (timeMS < eventPrintCutoff) {
                            std::cout<<"time "<<timeMS<<"ms: Process "<<running.process.getId()<<" started using the CPU for remaining "<<running.remainingCpuTime<<"ms of "<<fullBurstTime<<"ms burst "<<formatQueue(readyQueue, runtime)<<std::endl;
                        }
                    }
                }

                std::vector<int> completedIoThisTime;
                for (std::size_t i = 0; i < ioCompletions.size(); ) {
                    if (ioCompletions[i].first == timeMS) {
                        completedIoThisTime.push_back(ioCompletions[i].second);
                        ioCompletions.erase(ioCompletions.begin() + i);
                    } else {
                        i++;
                    }
                }
                std::sort(completedIoThisTime.begin(), completedIoThisTime.end(),
                    [&runtime](int a, int b) {
                        return runtime[a].process.getId() < runtime[b].process.getId();
                    });
                for (int processIndex : completedIoThisTime) {
                    addReadyProcess(processIndex, timeMS, readyQueue, runtime);
                    if (timeMS < eventPrintCutoff) {
                        std::cout<<"time "<<timeMS<<"ms: Process "<<runtime[processIndex].process.getId()<<" completed I/O; added to ready queue "<<formatQueue(readyQueue, runtime)<<std::endl;
                    }
                }

                std::vector<int> arrivalsThisTime;
                for (std::size_t i = 0; i < runtime.size(); i++) {
                    if (!runtime[i].arrived && runtime[i].process.getArrivalTime() == timeMS) {
                        arrivalsThisTime.push_back(static_cast<int>(i));
                    }
                }
                std::sort(arrivalsThisTime.begin(), arrivalsThisTime.end(),
                    [&runtime](int a, int b) {
                        return runtime[a].process.getId() < runtime[b].process.getId();
                    });
                for (int processIndex : arrivalsThisTime) {
                    runtime[processIndex].arrived = true;
                    addReadyProcess(processIndex, timeMS, readyQueue, runtime);
                    if (timeMS < eventPrintCutoff) {
                        std::cout<<"time "<<timeMS<<"ms: Process "<<runtime[processIndex].process.getId()<<" arrived; added to ready queue "<<formatQueue(readyQueue, runtime)<<std::endl;
                    }
                }

                if (currentProcess != -1) {
                    RuntimeProcess& running = runtime[currentProcess];
                    int elapsed = timeMS - cpuStartTime;

                    if (elapsed > 0 && elapsed % tslice == 0) {
                        running.remainingCpuTime -= tslice;
                        cpuBusyTime += tslice;
                        if (running.process.isIoBound()) {
                            ioBoundCpuBusyTime += tslice;
                        } else {
                            cpuBoundCpuBusyTime += tslice;
                        }

                        if (readyQueue.empty()) {
                            if (timeMS < eventPrintCutoff) {
                                std::cout<<"time "<<timeMS<<"ms: Time slice expired; no preemption because ready queue is empty "<<formatQueue(readyQueue, runtime)<<std::endl;
                            }
                            cpuStartTime = timeMS;
                        } else {
                            if (running.process.isIoBound()) {
                                rrStats.ioBoundPreemptions++;
                            } else {
                                rrStats.cpuBoundPreemptions++;
                            }

                            std::cout<<"time "<<timeMS<<"ms: Time slice expired; preempting process "<<running.process.getId()<<" with "<<running.remainingCpuTime<<"ms remaining "<<formatQueue(readyQueue, runtime)<<std::endl;
                            addReadyProcess(currentProcess, timeMS, readyQueue, runtime);
                            switchOutCompleteTime = timeMS + tcs / 2;
                            currentProcess = -1;
                            cpuStartTime = -1;
                        }
                    }
                }

                if (nextProcess == -1 && currentProcess == -1 && !readyQueue.empty()) {
                    nextProcess = readyQueue.front();
                    readyQueue.pop();
                    contextSwitchCompleteTime = std::max(timeMS, switchOutCompleteTime) + tcs / 2;
                }

                if (terminatedCount == static_cast<int>(runtime.size())) {
                    break;
                }
            } while(++timeMS);

            lastEventTime = std::max(lastEventTime, timeMS + tcs / 2);
            rrStats.cpuBoundContextSwitches = cpuBoundBurstCount + rrStats.cpuBoundPreemptions;
            rrStats.ioBoundContextSwitches = ioBoundBurstCount + rrStats.ioBoundPreemptions;
            rrStats.overallContextSwitches = rrStats.cpuBoundContextSwitches + rrStats.ioBoundContextSwitches;
            rrStats.overallPreemptions = rrStats.cpuBoundPreemptions + rrStats.ioBoundPreemptions;
            cpuBoundWaitTotal = cpuBoundTurnaroundTotal - cpuBoundCpuBusyTime - static_cast<long long>(rrStats.cpuBoundContextSwitches) * tcs;
            ioBoundWaitTotal = ioBoundTurnaroundTotal - ioBoundCpuBusyTime - static_cast<long long>(rrStats.ioBoundContextSwitches) * tcs;
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
