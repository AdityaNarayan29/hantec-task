#pragma once

#include "models/TradeRequest.h"
#include "models/TradeResult.h"
#include "processor/DealProcessor.h"

#include <string>
#include <vector>
#include <random>
#include <functional>
#include <atomic>
#include <mutex>

/// Simulates a client sending trade requests to the Deal Processor.
/// Each client runs in its own thread, generating random trade requests.
///
/// Configurable:
///   - Number of requests to send
///   - Delay between requests (simulates real client pacing)
///   - Whether to include intentional bad requests (for error handling demo)
class ClientSimulator {
public:
    struct Config {
        std::string clientId;
        int         numRequests     = 10;
        int         minDelayMs      = 50;   // Min delay between requests
        int         maxDelayMs      = 200;  // Max delay between requests
        bool        sendBadRequests = true;  // Include some invalid requests
    };

    explicit ClientSimulator(const Config& config);

    /// Run the client simulation. Submits all requests to the processor.
    /// This method is designed to be called from a std::thread.
    void run(DealProcessor& processor);

    /// Get results received by this client
    std::vector<TradeResult> getResults() const;

    /// Get the client ID
    const std::string& clientId() const { return config_.clientId; }

private:
    TradeRequest generateRequest();
    TradeRequest generateBadRequest();

    Config config_;

    std::vector<TradeResult> results_;
    mutable std::mutex resultsMutex_;

    std::mt19937 rng_;
    std::vector<std::string> symbols_ = {
        "EURUSD", "GBPUSD", "USDJPY", "AUDUSD", "USDCAD", "XAUUSD"
    };
};
