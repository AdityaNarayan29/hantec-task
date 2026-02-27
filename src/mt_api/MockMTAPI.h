#pragma once

#include "mt_api/IMTBrokerAPI.h"
#include <unordered_map>
#include <mutex>
#include <random>
#include <atomic>

/// Mock implementation of the MT5 Manager API for demo/testing.
///
/// Simulates realistic broker behavior:
/// - Known symbols with bid/ask spreads
/// - Account margin tracking (decreases with each trade)
/// - Random execution delays (simulates network + server processing)
/// - Configurable failure rate for rejection testing
/// - Thread-safe (multiple workers can call executeTrade concurrently)
class MockMTAPI : public IMTBrokerAPI {
public:
    explicit MockMTAPI(double failureRate = 0.05);

    bool connect(const std::string& server, int login, const std::string& password) override;
    void disconnect() override;
    bool isConnected() const override;

    std::optional<SymbolInfo>  getSymbolInfo(const std::string& symbol) override;
    std::optional<AccountInfo> getAccountInfo(int login) override;
    TradeResult                executeTrade(const TradeRequest& request) override;
    std::optional<TradeResult> getTicketInfo(const std::string& ticketId) override;
    std::vector<std::string>   getSymbols() override;

private:
    double generatePrice(const std::string& symbol, TradeType type);
    std::string generateTicketId();
    void simulateLatency();
    bool shouldFail();

    bool                    connected_ = false;
    double                  failureRate_;
    std::atomic<uint64_t>   ticketCounter_{100000};

    // Symbol database with base prices
    std::unordered_map<std::string, SymbolInfo> symbols_;

    // Simulated account state
    AccountInfo account_;
    mutable std::mutex accountMutex_;

    // Executed trades stored for getTicketInfo lookup
    std::unordered_map<std::string, TradeResult> executedTrades_;
    mutable std::mutex tradesMutex_;

    // Random number generation (per-thread safe via thread_local in .cpp)
    std::mt19937 rng_;
    std::uniform_real_distribution<double> failDist_{0.0, 1.0};
    std::uniform_int_distribution<int> latencyDist_{10, 100};
    mutable std::mutex rngMutex_;
};
