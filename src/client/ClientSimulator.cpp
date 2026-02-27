#include "client/ClientSimulator.h"
#include <thread>

ClientSimulator::ClientSimulator(const Config& config)
    : config_(config)
    , rng_(std::random_device{}())
{}

void ClientSimulator::run(DealProcessor& processor) {
    std::uniform_int_distribution<int> delayDist(config_.minDelayMs, config_.maxDelayMs);
    std::uniform_real_distribution<double> badChance(0.0, 1.0);

    for (int i = 0; i < config_.numRequests; ++i) {
        // 10% chance of sending a bad request (to test error handling)
        TradeRequest request;
        if (config_.sendBadRequests && badChance(rng_) < 0.10) {
            request = generateBadRequest();
        } else {
            request = generateRequest();
        }

        // Submit to processor with callback to capture the result
        processor.submit(request, [this](const TradeResult& result) {
            std::lock_guard<std::mutex> lock(resultsMutex_);
            results_.push_back(result);
        });

        // Simulate delay between client requests
        int delay = delayDist(rng_);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
}

std::vector<TradeResult> ClientSimulator::getResults() const {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    return results_;
}

TradeRequest ClientSimulator::generateRequest() {
    std::uniform_int_distribution<int> symbolDist(0, static_cast<int>(symbols_.size()) - 1);
    std::uniform_int_distribution<int> typeDist(0, 1);
    std::uniform_int_distribution<int> volumeDist(1, 50);   // 0.01 to 0.50 lots
    std::uniform_int_distribution<int> slTpChance(0, 100);

    TradeRequest req;
    req.clientId  = config_.clientId;
    req.requestId = TradeRequest::generateRequestId(config_.clientId);
    req.tradeType = typeDist(rng_) == 0 ? TradeType::BUY : TradeType::SELL;
    req.symbol    = symbols_[symbolDist(rng_)];
    req.volume    = volumeDist(rng_) * 0.01;  // In lot increments of 0.01
    req.timestamp = std::chrono::system_clock::now();

    // 40% chance to include SL/TP
    if (slTpChance(rng_) < 40) {
        double basePrice = (req.symbol == "XAUUSD") ? 2035.0 :
                           (req.symbol == "USDJPY") ? 149.0 : 1.0;
        double offset = basePrice * 0.005; // 0.5% offset
        if (req.tradeType == TradeType::BUY) {
            req.stopLoss   = basePrice - offset;
            req.takeProfit = basePrice + offset;
        } else {
            req.stopLoss   = basePrice + offset;
            req.takeProfit = basePrice - offset;
        }
    }

    return req;
}

TradeRequest ClientSimulator::generateBadRequest() {
    std::uniform_int_distribution<int> errorType(0, 3);

    TradeRequest req;
    req.clientId  = config_.clientId;
    req.requestId = TradeRequest::generateRequestId(config_.clientId);
    req.timestamp = std::chrono::system_clock::now();

    req.isTestBadRequest = true;

    switch (errorType(rng_)) {
        case 0:
            // Invalid symbol
            req.tradeType = TradeType::BUY;
            req.symbol    = "INVALID";
            req.volume    = 0.1;
            break;
        case 1:
            // Zero volume
            req.tradeType = TradeType::SELL;
            req.symbol    = "EURUSD";
            req.volume    = 0.0;
            break;
        case 2:
            // Volume too large
            req.tradeType = TradeType::BUY;
            req.symbol    = "EURUSD";
            req.volume    = 999.0;
            break;
        case 3:
            // Negative stop loss
            req.tradeType = TradeType::SELL;
            req.symbol    = "GBPUSD";
            req.volume    = 0.1;
            req.stopLoss  = -1.0;
            break;
    }

    return req;
}
