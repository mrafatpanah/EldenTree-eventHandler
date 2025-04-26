#ifndef ELDENTREE_HPP
#define ELDENTREE_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace elden_tree::god
{

    using GodId = uint64_t;
    using GodName = std::string;
    using LandId = uint64_t;

    // More descriptive Event structure
    struct GodEvent
    {
        GodId sourceGodId = 0;
        LandId targetLandId = 0;
        uint32_t eventTypeId = 0;
        std::string info;
    };

    // Interface for entities that process events (Lands/Sinks)
    class IEventProcessor
    {
    public:
        virtual ~IEventProcessor() = default;
        virtual void processEvent(const GodEvent &event) = 0;
        virtual LandId getLandId() const = 0;
    };

    using GodAction = std::function<void(const GodEvent &)>;

    struct God
    {
        GodId id;
        GodName name;

        bool operator==(const God &other) const { return id == other.id; }
        bool operator<(const God &other) const { return id < other.id; }
    };

} // namespace elden_tree::god

namespace std
{
    template <>
    struct hash<elden_tree::god::God>
    {
        std::size_t operator()(const elden_tree::god::God &god) const noexcept
        {
            return std::hash<elden_tree::god::GodId>{}(god.id);
        }
    };
} // namespace std

namespace elden_tree
{

    // Using aliases for clarity
    using god::GodEvent;
    using god::GodId;
    using god::GodName;
    using god::IEventProcessor;
    using god::LandId;

    // Thread-safe queue
    template <typename T>
    class ThreadSafeQueue
    {
    private:
        std::queue<T> queue_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::atomic<bool> &stop_token_;

    public:
        ThreadSafeQueue(std::atomic<bool> &stop) : stop_token_(stop) {}

        void push(T value)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                queue_.push(std::move(value));
            } // Lock released before notification
            cv_.notify_one();
        }

        // Waits until item available or stop requested
        bool waitAndPop(T &value)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]
                     { return !queue_.empty() || stop_token_.load(std::memory_order_acquire); });

            // Check if woken by stop signal or actual item
            if (stop_token_.load(std::memory_order_acquire) && queue_.empty())
            {
                return false;
            }
            value = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        bool empty() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.empty();
        }

        // Notify all waiting threads
        void notifyAllForStop()
        {
            cv_.notify_all();
        }
    };

    /**
     * @brief Manages event dispatching between Gods (sources) and Lands (processors).
     */
    class EldenTree final
    {
    public:
        /**
         * @brief Construct the EldenTree and starts worker threads.
         * @param numThreads Number of worker threads. Defaults to hardware concurrency.
         */
        explicit EldenTree(unsigned int numThreads = std::thread::hardware_concurrency());

        /**
         * @brief Destructor. Stops the event processing and joins threads.
         */
        ~EldenTree();

        // Disable copy/move semantics
        EldenTree(const EldenTree &) = delete;
        EldenTree &operator=(const EldenTree &) = delete;
        EldenTree(EldenTree &&) = delete;
        EldenTree &operator=(EldenTree &&) = delete;

        /**
         * @brief Registers an event processor (Land/Sink).
         * @param processor A shared pointer to the object implementing IEventProcessor.
         * @throws std::runtime_error if a processor with the same LandId already exists.
         */
        void registerProcessor(std::shared_ptr<IEventProcessor> processor);

        /**
         * @brief Unregisters an event processor.
         * @param landId The ID of the processor to unregister.
         */
        void unregisterProcessor(LandId landId);

        /**
         * @brief Posts an event to be processed by the target Land.
         * @param event The event to post.
         */
        void postEvent(GodEvent event) noexcept;

        /**
         * @brief Stops the event processing threads gracefully.
         * Can be called multiple times, subsequent calls have no effect.
         */
        void stop();

    private:
        /**
         * @brief The main loop executed by each worker thread.
         */
        void workerLoop();

        // --- Member Variables ---

        // Configuration
        unsigned int _numThreads;

        // Processors (Lands/Sinks)
        std::unordered_map<LandId, std::shared_ptr<IEventProcessor>> _processors;
        std::mutex _processorsMutex; // Protects access to _processors map

        // Event Queues - One queue per registered Land for fairness
        std::unordered_map<LandId, std::unique_ptr<ThreadSafeQueue<GodEvent>>> _eventQueues;
        std::mutex _queuesMutex; // Protects access to the _eventQueues map itself (not individual queues)

        // Thread pool management
        std::vector<std::thread> _workerThreads;
        std::atomic<bool> _stopRequested{false}; // Signal for threads to stop
    };

} // namespace elden_tree

#endif // ELDENTREE_HPP
