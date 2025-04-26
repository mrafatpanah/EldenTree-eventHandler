#include "elden_tree/EldenTree.hpp"
#include <benchmark/benchmark.h>

#include <atomic>
#include <memory>
#include <mutex> 
#include <thread>
#include <vector>
#include <iostream>

using namespace elden_tree;
using namespace elden_tree::god;

// --- Simple Processor for Benchmarking ---
// Minimal work to avoid processor being the bottleneck unless intended.
class BenchmarkingProcessor : public IEventProcessor {
private:
    LandId id_;
    std::atomic<int> eventCount_{0}; // Optional: Track count if needed

public:
    BenchmarkingProcessor(LandId id) : id_(id) {}

    void processEvent(const GodEvent& event) override {
        eventCount_.fetch_add(1, std::memory_order_relaxed);
    }

    LandId getLandId() const override {
        return id_;
    }

    int getEventCount() const {
        return eventCount_.load();
    }
};

// --- Benchmark Fixture ---
// Sets up the EldenTree instance and processors once for a set of benchmarks
class EldenTreeBenchmark : public ::benchmark::Fixture {
public:
    std::unique_ptr<EldenTree> tree;
    std::vector<std::shared_ptr<BenchmarkingProcessor>> processors;
    const unsigned int numTreeThreads = 4;
    const unsigned int numProcessors = 10;

    static std::mutex setupMutex;

    void SetUp(::benchmark::State& state) override {
        std::lock_guard<std::mutex> lock(setupMutex);

        // Run once per benchmark repetition set
        tree = std::make_unique<EldenTree>(numTreeThreads);
        processors.clear();
        for (unsigned int i = 0; i < numProcessors; ++i) {
            LandId landId = 100 + i;
            auto processor = std::make_shared<BenchmarkingProcessor>(landId);
            processors.push_back(processor);
            try {
                 tree->registerProcessor(processor);
            } catch(const std::exception& e) {
                std::cerr << "Benchmark setup failed: " << e.what() << std::endl;
                state.SkipWithError("Processor registration failed");
                return; 
            }
        }
    }

    void TearDown(const ::benchmark::State& state) override {
        processors.clear();
        tree.reset();
    }
};

std::mutex EldenTreeBenchmark::setupMutex;

// --- Benchmark Case: Event Posting Throughput ---
BENCHMARK_DEFINE_F(EldenTreeBenchmark, BM_PostEventThroughput)(benchmark::State& state) {

    // Determine target LandId based on thread index (if running multi-threaded benchmark)
    LandId targetLandId = 100 + (state.thread_index() % numProcessors);
    GodId sourceGodId = 500 + state.thread_index();

    // The main benchmark loop
    for (auto _ : state) {
        GodEvent event;
        event.sourceGodId = sourceGodId;
        event.targetLandId = targetLandId;
        event.info = "Benchmark event";

        // The operation to benchmark: posting an event
        tree->postEvent(std::move(event));
    }

    // Set items processed per iteration (usually 1 if you process one item)
    state.SetItemsProcessed(state.iterations());

    if (state.thread_index() == 0) { 
        long totalProcessed = 0;
        for(const auto& p : processors) {
            totalProcessed += p->getEventCount();
        }
        state.counters["TotalProcessed"] = totalProcessed;
    }
}

// Register the benchmark function defined using the fixture.
// Run with different numbers of producer threads posting events concurrently.
BENCHMARK_REGISTER_F(EldenTreeBenchmark, BM_PostEventThroughput)
    ->Threads(1)    // Run with 1 producer thread
    ->Threads(2)    // Run with 2 producer threads
    ->Threads(4)    // Run with 4 producer threads
    ->Threads(8)    // Run with 8 producer threads
    ->Threads(16)   // Run with 16 producer threads
    ->UseRealTime(); // Measure wall time, often better for concurrent code

// --- Google Benchmark Main Function ---
BENCHMARK_MAIN();


