#include "elden_tree/EldenTree.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace elden_tree;
using namespace elden_tree::god;

#define ZEUS_ID 1
#define HADES_ID 2
#define POSEIDON_ID 3

#define OLYMPUS_ID 101
#define UNDERWORLD_ID 102
#define SEA_ID 103

// --- Example Event Processor Implementation ---
class MyLandProcessor : public IEventProcessor {
private:
    LandId id_;
    std::string name_;

public:
    MyLandProcessor(LandId id, std::string name) : id_(id), name_(std::move(name)) {}

    void processEvent(const GodEvent& event) override {
        std::cout << "Land [" << name_ << " (" << id_ << ")] processing event from God "
                  << event.sourceGodId << ". Info: '" << event.info << "'"
                  << " (Thread: " << std::this_thread::get_id() << ")" << std::endl;

        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(10 + (rand() % 50)));
    }

    LandId getLandId() const override {
        return id_;
    }
};

// --- Main Function ---
int main() {
    std::cout << "--- EldenTree Improved Example ---" << std::endl;

    // Create the EldenTree with multiple worker threads
    EldenTree tree(4);

    // Create and register Processors (Lands)
    auto landOlympus = std::make_shared<MyLandProcessor>(OLYMPUS_ID, "Olympus");
    auto landUnderworld = std::make_shared<MyLandProcessor>(UNDERWORLD_ID, "Underworld");
    auto landSea = std::make_shared<MyLandProcessor>(SEA_ID, "The Sea");

    try {
        tree.registerProcessor(landOlympus);
        tree.registerProcessor(landUnderworld);
        tree.registerProcessor(landSea);
    } catch (const std::exception& e) {
        std::cerr << "Error registering processor: " << e.what() << std::endl;
        return 1;
    }

    // Simulate Gods posting events
    std::cout << "\n--- Posting Events ---" << std::endl;
    srand(time(0)); 

    std::vector<std::thread> producerThreads;

    // Simulate Zeus sending many events quickly
    producerThreads.emplace_back([&]() {
        for (int i = 0; i < 20; ++i) {
            GodEvent event;
            event.sourceGodId = ZEUS_ID;
            event.targetLandId = OLYMPUS_ID; // Target Olympus
            event.info = "Zeus Event #" + std::to_string(i);
            tree.postEvent(event);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
         std::cout << "* Zeus finished posting *" << std::endl;
    });

    // Simulate Hades sending events less frequently
     producerThreads.emplace_back([&]() {
        for (int i = 0; i < 10; ++i) {
            GodEvent event;
            event.sourceGodId = HADES_ID;
            event.targetLandId = UNDERWORLD_ID;
            event.info = "Hades Event #" + std::to_string(i);
            tree.postEvent(event);
             std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
         std::cout << "* Hades finished posting *" << std::endl;
    });

     // Simulate Poseidon sending events to multiple lands
      producerThreads.emplace_back([&]() {
        for (int i = 0; i < 15; ++i) {
            GodEvent event;
            event.sourceGodId = POSEIDON_ID;
            // Alternate targets
            event.targetLandId = (i % 2 == 0) ? SEA_ID : OLYMPUS_ID;
            event.info = "Poseidon Event #" + std::to_string(i);
            tree.postEvent(event);
             std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
         std::cout << "* Poseidon finished posting *" << std::endl;
    });


    // Wait for producer threads to finish posting
    for(auto& t : producerThreads) {
        if(t.joinable()) t.join();
    }

    // Let the events process for a while
    std::cout << "\n--- Waiting for events to process... ---" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Stop the handler gracefully
    std::cout << "\n--- Shutting down EldenTree ---" << std::endl;
    tree.stop();

    std::cout << "\n--- EldenTree Example Finished ---" << std::endl;
    return 0;
}
