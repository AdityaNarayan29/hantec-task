#pragma once

#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>

enum class TradeStatus {
    SUCCESS,
    REJECTED,
    INVALID_PARAMS,
    CONNECTION_ERROR,
    MARGIN_ERROR,
    DUPLICATE,
    RETRY_EXHAUSTED
};

struct TradeResult {
    std::string requestId;
    std::string clientId;
    TradeStatus status;
    std::string mtTicketId;      // MT5 deal ticket (empty on failure)
    double      executionPrice;   // Fill price (0.0 on failure)
    std::string errorMessage;
    int         retryCount;
    std::chrono::system_clock::time_point timestamp;

    std::string statusStr() const {
        switch (status) {
            case TradeStatus::SUCCESS:          return "SUCCESS";
            case TradeStatus::REJECTED:         return "REJECTED";
            case TradeStatus::INVALID_PARAMS:   return "INVALID_PARAMS";
            case TradeStatus::CONNECTION_ERROR:  return "CONNECTION_ERROR";
            case TradeStatus::MARGIN_ERROR:     return "MARGIN_ERROR";
            case TradeStatus::DUPLICATE:        return "DUPLICATE";
            case TradeStatus::RETRY_EXHAUSTED:  return "RETRY_EXHAUSTED";
        }
        return "UNKNOWN";
    }

    bool isSuccess() const { return status == TradeStatus::SUCCESS; }

    bool isRetryable() const {
        return status == TradeStatus::CONNECTION_ERROR ||
               status == TradeStatus::REJECTED;
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "[" << requestId << "] " << statusStr();
        if (isSuccess()) {
            oss << " Ticket=#" << mtTicketId
                << " Price=" << std::fixed << std::setprecision(5) << executionPrice;
        } else {
            oss << " Error: " << errorMessage;
        }
        if (retryCount > 0) {
            oss << " (retries=" << retryCount << ")";
        }
        return oss.str();
    }
};
