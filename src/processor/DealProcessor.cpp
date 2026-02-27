#include "processor/DealProcessor.h"
#include <sstream>

DealProcessor::DealProcessor(IMTBrokerAPI& api, Logger& logger, const ProcessorConfig& config)
    : api_(api)
    , logger_(logger)
    , config_(config)
    , validator_(api, logger)
{}

DealProcessor::~DealProcessor() {
    if (running_) {
        stop();
    }
}

void DealProcessor::start() {
    if (running_) return;

    running_ = true;
    logger_.info("DealProcessor starting with " + std::to_string(config_.numWorkers) + " worker threads");

    workers_.reserve(config_.numWorkers);
    for (int i = 0; i < config_.numWorkers; ++i) {
        workers_.emplace_back(&DealProcessor::workerLoop, this, i);
    }

    logger_.info("DealProcessor started successfully");
}

void DealProcessor::submit(TradeRequest request, ResultCallback callback) {
    if (!running_) {
        logger_.error("Cannot submit request - processor not running: " + request.requestId);
        return;
    }

    logger_.info("Request received: " + request.toString());
    queue_.push({std::move(request), std::move(callback)});
}

void DealProcessor::stop() {
    if (!running_) return;

    logger_.info("DealProcessor shutting down... draining queue (" +
                 std::to_string(queue_.size()) + " pending)");

    running_ = false;
    queue_.shutdown();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();

    logger_.info("DealProcessor stopped. All workers joined.");
}

void DealProcessor::workerLoop(int workerId) {
    std::string workerName = "Worker-" + std::to_string(workerId);
    logger_.info(workerName + " started");

    while (true) {
        auto item = queue_.pop();
        if (!item) {
            // Queue shutdown signaled and empty
            break;
        }

        auto& [request, callback] = *item;
        TradeResult result = processRequest(request, workerId);

        // Track result
        tracker_.record(result);

        // Notify client via callback if provided
        if (callback) {
            callback(result);
        }
    }

    logger_.info(workerName + " stopped");
}

TradeResult DealProcessor::processRequest(const TradeRequest& request, int workerId) {
    std::string workerName = "Worker-" + std::to_string(workerId);

    // Step 1: Validate the request before hitting the MT API
    logger_.info(workerName + " validating: " + request.requestId);
    auto validationError = validator_.validate(request);
    if (validationError) {
        logger_.warn(workerName + " validation failed: " + validationError->toString());
        return *validationError;
    }
    logger_.info(workerName + " validation passed: " + request.requestId);

    // Step 2: Execute trade (with retry logic for transient failures)
    TradeResult result = executeWithRetry(request, workerId);

    // Step 3: Log the final result
    if (result.isSuccess()) {
        logger_.info(workerName + " EXECUTED: " + result.toString());
    } else {
        logger_.error(workerName + " FAILED: " + result.toString());
    }

    return result;
}

TradeResult DealProcessor::executeWithRetry(const TradeRequest& request, int workerId) {
    std::string workerName = "Worker-" + std::to_string(workerId);
    TradeResult result;

    for (int attempt = 0; attempt <= config_.maxRetries; ++attempt) {
        if (attempt > 0) {
            // Exponential backoff: 100ms, 200ms, 400ms, ...
            int delayMs = config_.retryBaseMs * (1 << (attempt - 1));
            logger_.warn(workerName + " retrying " + request.requestId +
                         " (attempt " + std::to_string(attempt + 1) + "/" +
                         std::to_string(config_.maxRetries + 1) +
                         ", delay=" + std::to_string(delayMs) + "ms)");
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }

        // Call MT API: DealerSend equivalent
        logger_.info(workerName + " executing via MT API (DealerSend): " + request.toString());
        result = api_.executeTrade(request);
        result.retryCount = attempt;

        if (result.isSuccess() || !result.isRetryable()) {
            // Success or permanent failure - don't retry
            return result;
        }

        // Transient failure - will retry
        logger_.warn(workerName + " transient failure: " + result.errorMessage);
    }

    // All retries exhausted
    result.status = TradeStatus::RETRY_EXHAUSTED;
    result.errorMessage = "All " + std::to_string(config_.maxRetries + 1) +
                          " attempts failed. Last error: " + result.errorMessage;
    result.retryCount = config_.maxRetries;
    return result;
}
