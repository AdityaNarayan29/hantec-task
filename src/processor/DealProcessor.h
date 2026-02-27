#pragma once

#include "queue/ThreadSafeQueue.h"
#include "mt_api/IMTBrokerAPI.h"
#include "logger/Logger.h"
#include "tracker/ResultTracker.h"
#include "processor/Validator.h"
#include "models/TradeRequest.h"
#include "models/TradeResult.h"

#include <vector>
#include <thread>
#include <atomic>
#include <functional>

/// Configuration for the Deal Processor
struct ProcessorConfig {
    int    numWorkers  = 4;      // Number of worker threads
    int    maxRetries  = 3;      // Max retry attempts for failed trades
    int    retryBaseMs = 100;    // Base delay for exponential backoff (ms)
};

/// Central Deal Processor - the core of the system.
///
/// Architecture:
///   - Receives trade requests via submit() from multiple client threads
///   - Enqueues them in a ThreadSafeQueue (lock-free producer side)
///   - N worker threads dequeue, validate, execute, and track results
///   - Each worker independently processes requests using the MT API
///   - Results are tracked and can be queried by clients
///
/// Threading model:
///   - Client threads -> push to queue (thread-safe)
///   - Worker threads -> pop from queue, process (thread-safe)
///   - Queue uses mutex + condition_variable for blocking pop
///   - Logger uses its own mutex for output serialization
///   - ResultTracker uses its own mutex for result storage
class DealProcessor {
public:
    using ResultCallback = std::function<void(const TradeResult&)>;

    DealProcessor(IMTBrokerAPI& api, Logger& logger, const ProcessorConfig& config = {});
    ~DealProcessor();

    /// Start the worker thread pool
    void start();

    /// Submit a trade request (thread-safe, called from client threads)
    void submit(TradeRequest request, ResultCallback callback = nullptr);

    /// Graceful shutdown: stop accepting, drain queue, join workers
    void stop();

    /// Access the result tracker for querying results
    ResultTracker& getTracker() { return tracker_; }

    /// Current queue depth
    size_t queueDepth() const { return queue_.size(); }

private:
    /// Worker thread main loop
    void workerLoop(int workerId);

    /// Process a single request: validate -> execute -> retry if needed -> track
    TradeResult processRequest(const TradeRequest& request, int workerId);

    /// Execute with retry logic (bonus feature)
    TradeResult executeWithRetry(const TradeRequest& request, int workerId);

    IMTBrokerAPI&                api_;
    Logger&                      logger_;
    ProcessorConfig              config_;
    ResultTracker                tracker_;
    Validator                    validator_;

    ThreadSafeQueue<std::pair<TradeRequest, ResultCallback>> queue_;
    std::vector<std::thread>     workers_;
    std::atomic<bool>            running_{false};
};
