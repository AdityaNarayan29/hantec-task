#pragma once

#include "models/TradeResult.h"

#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>

/// Thread-safe result tracker.
/// Maintains the mapping between client request IDs and MT ticket IDs (bonus requirement).
/// Allows querying results by request ID or client ID.
class ResultTracker {
public:
    void record(const TradeResult& result);

    std::optional<TradeResult> getByRequestId(const std::string& requestId) const;
    std::vector<TradeResult>   getByClientId(const std::string& clientId) const;

    // Summary statistics
    struct Stats {
        int totalRequests  = 0;
        int successful     = 0;
        int rejected       = 0;
        int errors         = 0;
        int duplicates     = 0;
    };

    Stats getStats() const;
    Stats getClientStats(const std::string& clientId) const;
    void  printSummary() const;

private:
    // request ID -> result
    std::unordered_map<std::string, TradeResult> results_;
    // client ID -> list of request IDs
    std::unordered_map<std::string, std::vector<std::string>> clientRequests_;

    mutable std::mutex mutex_;
};
