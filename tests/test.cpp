#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "elden_tree/EldenTree.hpp"

using namespace elden_tree;
using namespace elden_tree::god;
using namespace std::chrono_literals;

// --- Mock Event Processor for Testing ---
class MockEventProcessor : public IEventProcessor
{
   private:
    LandId id_;
    std::string name_;
    std::atomic<int> eventCount_{0};
    std::map<GodId, std::string> lastEventInfoPerGod_;
    std::mutex mapMutex_;  // Protects access to the map

   public:
    MockEventProcessor(LandId id, std::string name)
        : id_(id), name_(std::move(name))
    {
    }

    void processEvent(const GodEvent& event) override
    {
        eventCount_++;
        {
            std::lock_guard<std::mutex> lock(mapMutex_);
            lastEventInfoPerGod_[event.sourceGodId] = event.info;
        }
        // Simulate some work
        std::this_thread::sleep_for(5ms +
                                    std::chrono::milliseconds(rand() % 10));
    }

    LandId getLandId() const override
    {
        return id_;
    }

    int getEventCount() const
    {
        return eventCount_.load();
    }

    std::string getLastEventInfo(GodId godId)
    {
        std::lock_guard<std::mutex> lock(mapMutex_);
        auto it = lastEventInfoPerGod_.find(godId);
        return (it != lastEventInfoPerGod_.end()) ? it->second : "";
    }

    void reset()
    {
        eventCount_ = 0;
        std::lock_guard<std::mutex> lock(mapMutex_);
        lastEventInfoPerGod_.clear();
    }
};

// --- Test Fixture for EldenTree Tests ---
class EldenTreeTest : public ::testing::Test
{
   protected:
    std::unique_ptr<EldenTree> tree;
    const unsigned int numTestThreads = 4;

    // Per-test-suite set-up.
    static void SetUpTestSuite()
    {
        srand(time(0));
    }

    // Per-test set-up.
    void SetUp() override
    {
        // Create a new tree instance for each test to ensure isolation
        tree = std::make_unique<EldenTree>(numTestThreads);
    }

    // Per-test tear-down.
    void TearDown() override
    {
        tree.reset();
    }
};

// --- Test Cases ---

// Test basic registration and unregistration of processors
TEST_F(EldenTreeTest, ProcessorRegistration)
{
    auto processor1 = std::make_shared<MockEventProcessor>(101, "TestLand1");
    auto processor2 = std::make_shared<MockEventProcessor>(102, "TestLand2");

    // Register processor1
    ASSERT_NO_THROW(tree->registerProcessor(processor1));

    // Re-registering the same ID should throw
    auto processor1_dup =
        std::make_shared<MockEventProcessor>(101, "TestLand1Dup");
    ASSERT_THROW(tree->registerProcessor(processor1_dup), std::runtime_error);

    // Register processor2
    ASSERT_NO_THROW(tree->registerProcessor(processor2));

    // Unregister processor1
    ASSERT_NO_THROW(tree->unregisterProcessor(101));

    // Unregistering non-existent processor should not throw (or could be
    // designed to)
    ASSERT_NO_THROW(tree->unregisterProcessor(999));

    // Unregister processor2
    ASSERT_NO_THROW(tree->unregisterProcessor(102));
}

// Test posting a single event and verifying it's processed
TEST_F(EldenTreeTest, SingleEventProcessing)
{
    auto processor =
        std::make_shared<MockEventProcessor>(201, "SingleEventLand");
    tree->registerProcessor(processor);

    GodEvent event;
    event.sourceGodId = 1;
    event.targetLandId = 201;
    event.info = "Test Event Alpha";

    tree->postEvent(event);

    std::this_thread::sleep_for(100ms);

    ASSERT_EQ(processor->getEventCount(), 1);
    ASSERT_EQ(processor->getLastEventInfo(1), "Test Event Alpha");
}

// Test posting multiple events to different processors
TEST_F(EldenTreeTest, MultipleEventsMultipleProcessors)
{
    auto processor1 = std::make_shared<MockEventProcessor>(301, "MultiLand1");
    auto processor2 = std::make_shared<MockEventProcessor>(302, "MultiLand2");
    tree->registerProcessor(processor1);
    tree->registerProcessor(processor2);

    constexpr int numEventsPerProcessor = 10;
    GodId godA = 10;
    GodId godB = 11;

    // Post events targeting processor1
    for (int i = 0; i < numEventsPerProcessor; ++i)
    {
        GodEvent ev;
        ev.sourceGodId = godA;
        ev.targetLandId = 301;
        ev.info = "P1 Event " + std::to_string(i);
        tree->postEvent(ev);
    }

    // Post events targeting processor2
    for (int i = 0; i < numEventsPerProcessor; ++i)
    {
        GodEvent ev;
        ev.sourceGodId = godB;
        ev.targetLandId = 302;
        ev.info = "P2 Event " + std::to_string(i);
        tree->postEvent(ev);
    }

    // Wait for events to process
    std::this_thread::sleep_for(500ms);

    EXPECT_EQ(processor1->getEventCount(), numEventsPerProcessor);
    EXPECT_EQ(processor1->getLastEventInfo(godA),
              "P1 Event " + std::to_string(numEventsPerProcessor - 1));

    EXPECT_EQ(processor2->getEventCount(), numEventsPerProcessor);
    EXPECT_EQ(processor2->getLastEventInfo(godB),
              "P2 Event " + std::to_string(numEventsPerProcessor - 1));
}

// Test basic fairness: ensure events for one processor don't completely starve
// another
TEST_F(EldenTreeTest, BasicFairnessTest)
{
    auto processorFast = std::make_shared<MockEventProcessor>(401, "FastLand");
    auto processorSlow = std::make_shared<MockEventProcessor>(
        402, "SlowLand");  // Simulate slower processing if needed
    tree->registerProcessor(processorFast);
    tree->registerProcessor(processorSlow);

    GodId godFast = 20;
    GodId godSlow = 21;
    constexpr int numFastEvents = 50;
    constexpr int numSlowEvents = 5;

    // Post many events to the fast processor
    for (int i = 0; i < numFastEvents; ++i)
    {
        GodEvent ev;
        ev.sourceGodId = godFast;
        ev.targetLandId = 401;
        ev.info = "Fast " + std::to_string(i);
        tree->postEvent(ev);
    }

    // Post a few events to the slow processor interspersed or afterwards
    for (int i = 0; i < numSlowEvents; ++i)
    {
        GodEvent ev;
        ev.sourceGodId = godSlow;
        ev.targetLandId = 402;
        ev.info = "Slow " + std::to_string(i);
        tree->postEvent(ev);
        std::this_thread::sleep_for(1ms);
    }

    // Wait long enough for *all* events to reasonably complete
    std::this_thread::sleep_for(1s);

    // Check that the slow processor received its events, even with the flood to
    // the fast one
    EXPECT_EQ(processorFast->getEventCount(), numFastEvents);
    EXPECT_EQ(processorSlow->getEventCount(), numSlowEvents);
    EXPECT_EQ(processorSlow->getLastEventInfo(godSlow),
              "Slow " + std::to_string(numSlowEvents - 1));
}

// Test posting events to an unregistered processor
TEST_F(EldenTreeTest, PostToUnregisteredProcessor)
{
    auto processor =
        std::make_shared<MockEventProcessor>(501, "RegisteredLand");
    tree->registerProcessor(processor);

    GodEvent eventRegistered;
    eventRegistered.sourceGodId = 1;
    eventRegistered.targetLandId = 501;
    eventRegistered.info = "Goes to registered";

    GodEvent eventUnregistered;
    eventUnregistered.sourceGodId = 2;
    eventUnregistered.targetLandId = 999;  // Non-existent Land ID
    eventUnregistered.info = "Goes nowhere";

    // Expect no exceptions when posting to unregistered (should be handled
    // gracefully, e.g., logged)
    ASSERT_NO_THROW(tree->postEvent(eventUnregistered));
    ASSERT_NO_THROW(tree->postEvent(eventRegistered));

    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(processor->getEventCount(),
              1);  // Only the registered event processed
}

// Test stopping the tree while events are pending
TEST_F(EldenTreeTest, StopWithPendingEvents)
{
    auto processor = std::make_shared<MockEventProcessor>(601, "StopTestLand");
    tree->registerProcessor(processor);

    constexpr int numEvents = 100;
    // Post many events quickly
    for (int i = 0; i < numEvents; ++i)
    {
        GodEvent ev;
        ev.sourceGodId = 1;
        ev.targetLandId = 601;
        ev.info = "Stop Test " + std::to_string(i);
        tree->postEvent(ev);
    }

    // Give a very short time for some processing to start
    std::this_thread::sleep_for(10ms);

    ASSERT_NO_THROW(tree->stop());

    // Check that not all events were necessarily processed
    int processedCount = processor->getEventCount();
    EXPECT_LT(processedCount, numEvents)
        << "Expected fewer than all events to be processed after quick stop.";
    EXPECT_GT(processedCount, 0)
        << "Expected at least some events to be processed before stop.";

    // Try posting after stop (should ideally be ignored or handled gracefully)
    GodEvent lateEvent;
    lateEvent.sourceGodId = 2;
    lateEvent.targetLandId = 601;
    lateEvent.info = "Late Event";
    ASSERT_NO_THROW(tree->postEvent(lateEvent));

    // Wait a bit more to ensure no late events are processed
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(processor->getEventCount(), processedCount)
        << "Event count should not increase after stop.";
}

// --- Google Test Main Function ---
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
