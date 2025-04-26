#include "elden_tree/EldenTree.hpp"

#include <iostream>
#include <vector>
#include <memory>

namespace elden_tree
{

    // --- Constructor ---
    EldenTree::EldenTree(unsigned int numThreads) : _numThreads(numThreads == 0 ? std::thread::hardware_concurrency() : numThreads),
                                                    _stopRequested(false)
    {
        if (_numThreads == 0)
            _numThreads = 1;

        std::cout << "Starting EldenTree with " << _numThreads << " worker threads..." << std::endl;
        _workerThreads.reserve(_numThreads);
        for (unsigned int i = 0; i < _numThreads; ++i)
        {
            // Launch threads executing the workerLoop member function
            _workerThreads.emplace_back(&EldenTree::workerLoop, this);
        }
        std::cout << "EldenTree started." << std::endl;
    }

    // --- Destructor ---
    EldenTree::~EldenTree()
    {
        stop();
    }

    // --- registerProcessor ---
    void EldenTree::registerProcessor(std::shared_ptr<IEventProcessor> processor)
    {
        if (!processor)
        {
            throw std::invalid_argument("Processor cannot be null.");
        }

        LandId landId = processor->getLandId();
        std::cout << "Registering processor for Land " << landId << std::endl;

        std::lock_guard<std::mutex> procLock(_processorsMutex);
        std::lock_guard<std::mutex> queueLock(_queuesMutex);

        // Check if processor already exists
        if (_processors.count(landId))
        {
            throw std::runtime_error("Processor with LandId " + std::to_string(landId) + " already registered.");
        }

        // Add processor
        _processors[landId] = processor;

        // Create a new event queue for this processor
        _eventQueues[landId] = std::make_unique<ThreadSafeQueue<GodEvent>>(_stopRequested);

        std::cout << "Processor for Land " << landId << " registered successfully." << std::endl;
    }

    // --- unregisterProcessor ---
    void EldenTree::unregisterProcessor(LandId landId)
    {
        std::cout << "Unregistering processor for Land " << landId << std::endl;
        // Lock both maps to ensure consistency
        std::lock_guard<std::mutex> procLock(_processorsMutex);
        std::lock_guard<std::mutex> queueLock(_queuesMutex);

        // Erase processor and its queue
        size_t processorsErased = _processors.erase(landId);
        size_t queuesErased = _eventQueues.erase(landId);

        if (processorsErased > 0 || queuesErased > 0)
        {
            std::cout << "Processor for Land " << landId << " unregistered." << std::endl;
        }
        else
        {
            std::cerr << "Warning: Attempted to unregister non-existent processor for Land " << landId << std::endl;
        }
    }

    // --- postEvent ---
    void EldenTree::postEvent(GodEvent event) noexcept
    {
        // Find the queue for the target Land
        std::unique_lock<std::mutex> lock(_queuesMutex);
        auto it = _eventQueues.find(event.targetLandId);
        if (it != _eventQueues.end())
        {
            // Get the specific queue pointer
            ThreadSafeQueue<GodEvent> *queuePtr = it->second.get();
            lock.unlock();

            // Push the event onto the specific Land's queue
            if (queuePtr)
            {
                queuePtr->push(std::move(event));
            }
            else
            {
                // Should not happen
                std::cerr << "Error: Queue pointer is null for Land " << event.targetLandId << std::endl;
            }
        }
        else
        {
            lock.unlock(); // Ensure lock is released
            std::cerr << "Warning: No registered processor for target Land " << event.targetLandId
                      << ". Event discarded." << std::endl;
        }
    }

    // --- stop ---
    void EldenTree::stop()
    {
        // Atomically check and set the stop flag
        bool alreadyStopping = _stopRequested.exchange(true, std::memory_order_acq_rel);

        if (alreadyStopping)
        {
            std::cout << "EldenTree stop already initiated." << std::endl;
            return;
        }

        std::cout << "Stopping EldenTree..." << std::endl;

        // --- Wake up all threads ---
        // Notify threads potentially waiting on individual queues
        {
            std::lock_guard<std::mutex> lock(_queuesMutex);
            for (auto const &[landId, queuePtr] : _eventQueues)
            {
                if (queuePtr)
                {
                    queuePtr->notifyAllForStop();
                }
            }
        }

        // --- Join threads ---
        std::cout << "Joining worker threads..." << std::endl;
        for (std::thread &t : _workerThreads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
        _workerThreads.clear();
        std::cout << "EldenTree stopped." << std::endl;
    }

    // --- workerLoop ---
    void EldenTree::workerLoop()
    {
        std::cout << "Worker thread [" << std::this_thread::get_id() << "] started." << std::endl;

        while (!(_stopRequested.load(std::memory_order_acquire)))
        {
            GodEvent event;
            bool eventFound = false;
            LandId processedLandId = 0;

            // --- Fairly check queues (Round-Robin Idea) ---
            std::vector<LandId> landIdsToCheck;
            {
                std::lock_guard<std::mutex> lock(_queuesMutex);
                for (auto const &[id, queue] : _eventQueues)
                {
                    if (queue && !queue->empty())
                    {
                        landIdsToCheck.push_back(id);
                    }
                }
            }

            // Iterate through the lands that might have events
            for (LandId currentLandId : landIdsToCheck)
            {
                if (_stopRequested.load(std::memory_order_acquire))
                    break;

                ThreadSafeQueue<GodEvent> *queuePtr = nullptr;
                {
                    std::lock_guard<std::mutex> lock(_queuesMutex);
                    auto it = _eventQueues.find(currentLandId);
                    if (it != _eventQueues.end())
                    {
                        queuePtr = it->second.get();
                    }
                }

                if (queuePtr)
                {
                    if (queuePtr->waitAndPop(event))
                    {
                        // Successfully got an event (or woken by stop, but queue wasn't empty)
                        eventFound = true;
                        processedLandId = currentLandId;
                        break;
                    }
                }
            }

            // --- Process the found event ---
            if (eventFound)
            {
                std::shared_ptr<IEventProcessor> processor = nullptr;
                {
                    // Lock processors map briefly to get the processor
                    std::lock_guard<std::mutex> lock(_processorsMutex);
                    auto it = _processors.find(processedLandId);
                    if (it != _processors.end())
                    {
                        processor = it->second; // Get shared_ptr
                    }
                } // Release processor lock

                if (processor)
                {
                    try
                    {
                        // Call the processor's method - NO LOCK HELD HERE
                        processor->processEvent(event);
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "Worker thread [" << std::this_thread::get_id()
                                  << "]: Exception during processEvent for Land " << processedLandId
                                  << ": " << e.what() << std::endl;
                        // Decide how to handle processor errors (e.g., log, disable processor?)
                    }
                    catch (...)
                    {
                        std::cerr << "Worker thread [" << std::this_thread::get_id()
                                  << "]: Unknown exception during processEvent for Land " << processedLandId
                                  << std::endl;
                    }
                }
                else
                {
                    std::cerr << "Warning: Worker thread [" << std::this_thread::get_id()
                              << "]: Processor for Land " << processedLandId
                              << " disappeared before event processing." << std::endl;
                }
            }
            else if (!_stopRequested.load(std::memory_order_acquire))
            {
                // No event found across all queues, wait a short time before checking again
                // This prevents busy-looping when idle.
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        std::cout << "Worker thread [" << std::this_thread::get_id() << "] stopping." << std::endl;
    }

} // namespace elden_tree
