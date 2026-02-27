#pragma once

#include "models/TradeRequest.h"
#include "models/TradeResult.h"
#include "mt_api/IMTBrokerAPI.h"
#include "logger/Logger.h"

#include <unordered_set>
#include <mutex>
#include <string>
#include <optional>

/// Pre-execution validation layer.
/// Checks requests BEFORE they reach the MT API, catching obvious errors early.
/// This mirrors what a production system would do before calling DealerSend().
class Validator {
public:
    Validator(IMTBrokerAPI& api, Logger& logger)
        : api_(api), logger_(logger) {}

    /// Validate a trade request. Returns a TradeResult with error details on failure,
    /// or std::nullopt if validation passes.
    std::optional<TradeResult> validate(const TradeRequest& request) {
        // 1. Check for duplicate request IDs
        {
            std::lock_guard<std::mutex> lock(dedupMutex_);
            if (seenRequests_.count(request.requestId)) {
                logger_.warn("Duplicate request detected: " + request.requestId);
                return makeError(request, TradeStatus::DUPLICATE,
                                 "Duplicate request ID: " + request.requestId);
            }
            seenRequests_.insert(request.requestId);
        }

        // 2. Basic parameter validation
        if (request.clientId.empty()) {
            return makeError(request, TradeStatus::INVALID_PARAMS, "Empty client ID");
        }

        if (request.symbol.empty()) {
            return makeError(request, TradeStatus::INVALID_PARAMS, "Empty symbol");
        }

        if (request.volume <= 0.0) {
            return makeError(request, TradeStatus::INVALID_PARAMS,
                             "Invalid volume: " + std::to_string(request.volume));
        }

        // 3. Symbol validation (calls SymbolGet equivalent)
        auto symbolInfo = api_.getSymbolInfo(request.symbol);
        if (!symbolInfo) {
            return makeError(request, TradeStatus::INVALID_PARAMS,
                             "Unknown symbol: " + request.symbol);
        }

        if (!symbolInfo->tradeAllowed) {
            return makeError(request, TradeStatus::REJECTED,
                             "Trading not allowed for: " + request.symbol);
        }

        // 4. Volume range check
        if (request.volume < symbolInfo->minVolume || request.volume > symbolInfo->maxVolume) {
            return makeError(request, TradeStatus::INVALID_PARAMS,
                             "Volume " + std::to_string(request.volume) +
                             " outside range [" + std::to_string(symbolInfo->minVolume) +
                             ", " + std::to_string(symbolInfo->maxVolume) + "]");
        }

        // 5. SL/TP sanity check (if provided)
        if (request.stopLoss && *request.stopLoss <= 0.0) {
            return makeError(request, TradeStatus::INVALID_PARAMS,
                             "Invalid stop loss: " + std::to_string(*request.stopLoss));
        }

        if (request.takeProfit && *request.takeProfit <= 0.0) {
            return makeError(request, TradeStatus::INVALID_PARAMS,
                             "Invalid take profit: " + std::to_string(*request.takeProfit));
        }

        // All checks passed
        return std::nullopt;
    }

private:
    TradeResult makeError(const TradeRequest& req, TradeStatus status, const std::string& msg) {
        TradeResult result;
        result.requestId = req.requestId;
        result.clientId = req.clientId;
        result.status = status;
        result.errorMessage = msg;
        result.executionPrice = 0.0;
        result.retryCount = 0;
        result.timestamp = std::chrono::system_clock::now();
        return result;
    }

    IMTBrokerAPI& api_;
    Logger& logger_;
    std::unordered_set<std::string> seenRequests_;
    std::mutex dedupMutex_;
};
