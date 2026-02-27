#include "mt_api/MockMTAPI.h"
#include <thread>
#include <cmath>
#include <sstream>
#include <iomanip>

MockMTAPI::MockMTAPI(double failureRate)
    : failureRate_(failureRate)
    , rng_(std::random_device{}())
{
    // Initialize symbol database with realistic forex pairs
    // These mirror what MT5 SymbolGet() would return from the server
    symbols_["EURUSD"] = {"EURUSD", 1.08450, 1.08465, 0.01, 100.0, 0.01, 5, true};
    symbols_["GBPUSD"] = {"GBPUSD", 1.26320, 1.26340, 0.01, 100.0, 0.01, 5, true};
    symbols_["USDJPY"] = {"USDJPY", 149.850, 149.865, 0.01, 100.0, 0.01, 3, true};
    symbols_["AUDUSD"] = {"AUDUSD", 0.65230, 0.65248, 0.01, 100.0, 0.01, 5, true};
    symbols_["USDCAD"] = {"USDCAD", 1.35720, 1.35738, 0.01, 100.0, 0.01, 5, true};
    symbols_["XAUUSD"] = {"XAUUSD", 2035.50, 2036.00, 0.01,  50.0, 0.01, 2, true};

    // Initialize demo account with $100,000 balance
    account_ = {12345, 100000.0, 100000.0, 100000.0, 0.0, "USD"};
}

bool MockMTAPI::connect(const std::string& server, int login, const std::string& password) {
    // Simulates IMTManagerAPI::Connect(server, login, password)
    simulateLatency();
    connected_ = true;
    account_.login = login;
    return true;
}

void MockMTAPI::disconnect() {
    // Simulates IMTManagerAPI::Disconnect()
    connected_ = false;
}

bool MockMTAPI::isConnected() const {
    return connected_;
}

std::optional<SymbolInfo> MockMTAPI::getSymbolInfo(const std::string& symbol) {
    // Simulates IMTManagerAPI::SymbolGet(symbol, &info)
    // followed by IMTManagerAPI::SymbolInfoGet(symbol, &tick) for live prices
    auto it = symbols_.find(symbol);
    if (it == symbols_.end()) return std::nullopt;

    // Add small random price variation to simulate live market
    SymbolInfo info = it->second;
    std::lock_guard<std::mutex> lock(rngMutex_);
    double variation = (failDist_(rng_) - 0.5) * 0.0010; // +/- 0.5 pips
    info.bid += variation;
    info.ask += variation;
    return info;
}

std::optional<AccountInfo> MockMTAPI::getAccountInfo(int login) {
    // Simulates IMTManagerAPI::UserAccountGet(login, &account)
    std::lock_guard<std::mutex> lock(accountMutex_);
    if (login != account_.login) return std::nullopt;
    return account_;
}

TradeResult MockMTAPI::executeTrade(const TradeRequest& request) {
    // Simulates IMTManagerAPI::DealerSend(&dealerRequest, &dealerAnswer)
    //
    // DealerSend is the correct method for manager-initiated trades because:
    // 1. It passes through ALL server-side validations (margin, symbol limits, sessions)
    // 2. The server checks trade permissions, price validity, and margin requirements
    // 3. It returns a proper deal ticket on success
    // 4. Unlike direct deal creation, it respects trading hours and symbol restrictions

    TradeResult result;
    result.requestId = request.requestId;
    result.clientId = request.clientId;
    result.retryCount = 0;
    result.timestamp = std::chrono::system_clock::now();

    // Simulate network + server processing delay
    simulateLatency();

    // Simulate random connection failure
    if (shouldFail()) {
        result.status = TradeStatus::CONNECTION_ERROR;
        result.errorMessage = "MT5 server connection timeout during DealerSend()";
        return result;
    }

    // Step 1: Symbol validation (SymbolGet check)
    auto symbolInfo = symbols_.find(request.symbol);
    if (symbolInfo == symbols_.end()) {
        result.status = TradeStatus::INVALID_PARAMS;
        result.errorMessage = "Symbol '" + request.symbol + "' not found (SymbolGet failed)";
        return result;
    }

    if (!symbolInfo->second.tradeAllowed) {
        result.status = TradeStatus::REJECTED;
        result.errorMessage = "Trading disabled for symbol '" + request.symbol + "'";
        return result;
    }

    // Step 2: Volume validation (server-side check in DealerSend)
    if (request.volume < symbolInfo->second.minVolume ||
        request.volume > symbolInfo->second.maxVolume) {
        result.status = TradeStatus::INVALID_PARAMS;
        result.errorMessage = "Volume " + std::to_string(request.volume) +
                              " outside allowed range [" +
                              std::to_string(symbolInfo->second.minVolume) + ", " +
                              std::to_string(symbolInfo->second.maxVolume) + "]";
        return result;
    }

    // Check volume step alignment (use rounding tolerance for floating-point)
    double steps = request.volume / symbolInfo->second.volumeStep;
    double rounded = std::round(steps);
    if (std::fabs(steps - rounded) > 1e-6) {
        result.status = TradeStatus::INVALID_PARAMS;
        result.errorMessage = "Volume " + std::to_string(request.volume) +
                              " not aligned to step " +
                              std::to_string(symbolInfo->second.volumeStep);
        return result;
    }

    // Step 3: Margin check (UserAccountGet -> margin validation in DealerSend)
    double requiredMargin = request.volume * 1000.0; // Simplified: $1000 per lot
    {
        std::lock_guard<std::mutex> lock(accountMutex_);
        if (account_.freeMargin < requiredMargin) {
            result.status = TradeStatus::MARGIN_ERROR;
            result.errorMessage = "Insufficient margin. Required: $" +
                                  std::to_string(requiredMargin) +
                                  ", Available: $" +
                                  std::to_string(account_.freeMargin);
            return result;
        }

        // Reserve margin
        account_.freeMargin -= requiredMargin;
        account_.equity -= requiredMargin * 0.001; // Small equity impact
    }

    // Step 4: Execute - generate fill price and ticket
    double price = generatePrice(request.symbol, request.tradeType);
    std::string ticket = generateTicketId();

    result.status = TradeStatus::SUCCESS;
    result.mtTicketId = ticket;
    result.executionPrice = price;

    // Store in executed trades map (for DealGet lookups later)
    {
        std::lock_guard<std::mutex> lock(tradesMutex_);
        executedTrades_[ticket] = result;
    }

    return result;
}

std::optional<TradeResult> MockMTAPI::getTicketInfo(const std::string& ticketId) {
    // Simulates IMTManagerAPI::DealGet(ticket, &deal)
    std::lock_guard<std::mutex> lock(tradesMutex_);
    auto it = executedTrades_.find(ticketId);
    if (it == executedTrades_.end()) return std::nullopt;
    return it->second;
}

std::vector<std::string> MockMTAPI::getSymbols() {
    // Simulates iterating via IMTManagerAPI::SymbolNext()
    std::vector<std::string> result;
    result.reserve(symbols_.size());
    for (const auto& [name, info] : symbols_) {
        result.push_back(name);
    }
    return result;
}

double MockMTAPI::generatePrice(const std::string& symbol, TradeType type) {
    auto it = symbols_.find(symbol);
    if (it == symbols_.end()) return 0.0;

    const auto& info = it->second;
    // BUY executes at ASK price, SELL executes at BID price
    double basePrice = (type == TradeType::BUY) ? info.ask : info.bid;

    // Add small slippage variation
    std::lock_guard<std::mutex> lock(rngMutex_);
    double slippage = (failDist_(rng_) - 0.5) * 0.00005;
    return basePrice + slippage;
}

std::string MockMTAPI::generateTicketId() {
    uint64_t id = ticketCounter_.fetch_add(1);
    return std::to_string(id);
}

void MockMTAPI::simulateLatency() {
    int ms;
    {
        std::lock_guard<std::mutex> lock(rngMutex_);
        ms = latencyDist_(rng_);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

bool MockMTAPI::shouldFail() {
    std::lock_guard<std::mutex> lock(rngMutex_);
    return failDist_(rng_) < failureRate_;
}
