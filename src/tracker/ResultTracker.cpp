#include "tracker/ResultTracker.h"
#include <iostream>
#include <iomanip>

void ResultTracker::record(const TradeResult& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    results_[result.requestId] = result;
    clientRequests_[result.clientId].push_back(result.requestId);
}

std::optional<TradeResult> ResultTracker::getByRequestId(const std::string& requestId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = results_.find(requestId);
    if (it == results_.end()) return std::nullopt;
    return it->second;
}

std::vector<TradeResult> ResultTracker::getByClientId(const std::string& clientId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TradeResult> results;
    auto it = clientRequests_.find(clientId);
    if (it == clientRequests_.end()) return results;

    for (const auto& reqId : it->second) {
        auto resIt = results_.find(reqId);
        if (resIt != results_.end()) {
            results.push_back(resIt->second);
        }
    }
    return results;
}

ResultTracker::Stats ResultTracker::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Stats stats;
    for (const auto& [id, result] : results_) {
        stats.totalRequests++;
        switch (result.status) {
            case TradeStatus::SUCCESS:         stats.successful++; break;
            case TradeStatus::DUPLICATE:       stats.duplicates++; break;
            case TradeStatus::REJECTED:
            case TradeStatus::MARGIN_ERROR:
            case TradeStatus::RETRY_EXHAUSTED: stats.rejected++;   break;
            case TradeStatus::CONNECTION_ERROR:
            case TradeStatus::INVALID_PARAMS:  stats.errors++;     break;
        }
    }
    return stats;
}

ResultTracker::Stats ResultTracker::getClientStats(const std::string& clientId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    Stats stats;
    auto it = clientRequests_.find(clientId);
    if (it == clientRequests_.end()) return stats;

    for (const auto& reqId : it->second) {
        auto resIt = results_.find(reqId);
        if (resIt == results_.end()) continue;
        stats.totalRequests++;
        switch (resIt->second.status) {
            case TradeStatus::SUCCESS:         stats.successful++; break;
            case TradeStatus::DUPLICATE:       stats.duplicates++; break;
            case TradeStatus::REJECTED:
            case TradeStatus::MARGIN_ERROR:
            case TradeStatus::RETRY_EXHAUSTED: stats.rejected++;   break;
            case TradeStatus::CONNECTION_ERROR:
            case TradeStatus::INVALID_PARAMS:  stats.errors++;     break;
        }
    }
    return stats;
}

void ResultTracker::printSummary() const {
    auto stats = getStats();

    std::cout << "\n"
              << "================================================================\n"
              << "                    EXECUTION SUMMARY\n"
              << "================================================================\n"
              << "  Total Requests:   " << stats.totalRequests << "\n"
              << "  Successful:       " << stats.successful << "\n"
              << "  Rejected:         " << stats.rejected << "\n"
              << "  Errors:           " << stats.errors << "\n"
              << "  Duplicates:       " << stats.duplicates << "\n"
              << "  Success Rate:     "
              << std::fixed << std::setprecision(1)
              << (stats.totalRequests > 0
                  ? (100.0 * stats.successful / stats.totalRequests) : 0.0)
              << "%\n"
              << "================================================================\n";

    // Per-client breakdown
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "\n  Per-Client Breakdown:\n";
    std::cout << "  " << std::left << std::setw(12) << "Client"
              << std::setw(8) << "Total"
              << std::setw(8) << "OK"
              << std::setw(8) << "Fail"
              << std::setw(8) << "Dup" << "\n";
    std::cout << "  " << std::string(44, '-') << "\n";

    for (const auto& [clientId, reqIds] : clientRequests_) {
        int ok = 0, fail = 0, dup = 0;
        for (const auto& reqId : reqIds) {
            auto it = results_.find(reqId);
            if (it == results_.end()) continue;
            if (it->second.isSuccess()) ok++;
            else if (it->second.status == TradeStatus::DUPLICATE) dup++;
            else fail++;
        }
        std::cout << "  " << std::left << std::setw(12) << clientId
                  << std::setw(8) << reqIds.size()
                  << std::setw(8) << ok
                  << std::setw(8) << fail
                  << std::setw(8) << dup << "\n";
    }

    // Request ID -> Ticket ID mapping (bonus feature)
    std::cout << "\n  Request ID -> MT Ticket Mapping (successful trades):\n";
    std::cout << "  " << std::left << std::setw(22) << "Request ID"
              << std::setw(12) << "Ticket"
              << "Price" << "\n";
    std::cout << "  " << std::string(50, '-') << "\n";
    for (const auto& [reqId, result] : results_) {
        if (result.isSuccess()) {
            std::cout << "  " << std::left << std::setw(22) << reqId
                      << std::setw(12) << ("#" + result.mtTicketId)
                      << std::fixed << std::setprecision(5) << result.executionPrice
                      << "\n";
        }
    }
    std::cout << "================================================================\n\n";
}
