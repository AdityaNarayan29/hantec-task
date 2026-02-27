#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <atomic>
#include <sstream>
#include <iomanip>

enum class TradeType { BUY, SELL };

struct TradeRequest {
    std::string clientId;
    std::string requestId;
    TradeType   tradeType;
    std::string symbol;
    double      volume;
    std::optional<double> stopLoss;
    std::optional<double> takeProfit;
    std::chrono::system_clock::time_point timestamp;
    bool isTestBadRequest = false;  // Flagged when intentionally invalid for error testing

    // Generate unique request IDs
    static std::string generateRequestId(const std::string& clientId) {
        static std::atomic<uint64_t> counter{0};
        std::ostringstream oss;
        oss << clientId << "-" << std::setfill('0') << std::setw(6) << counter.fetch_add(1);
        return oss.str();
    }

    std::string tradeTypeStr() const {
        return tradeType == TradeType::BUY ? "BUY" : "SELL";
    }

    std::string toString() const {
        std::ostringstream oss;
        if (isTestBadRequest) oss << "[INTENTIONAL-BAD-REQUEST] ";
        oss << "[" << requestId << "] "
            << clientId << " " << tradeTypeStr() << " "
            << symbol << " " << volume << " lots";
        if (stopLoss)   oss << " SL=" << *stopLoss;
        if (takeProfit) oss << " TP=" << *takeProfit;
        return oss.str();
    }
};
