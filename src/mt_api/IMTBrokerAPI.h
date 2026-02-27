#pragma once

#include <string>
#include <vector>
#include "models/TradeRequest.h"
#include "models/TradeResult.h"

/// Symbol specification returned by MT5 SymbolGet() / SymbolInfoGet()
struct SymbolInfo {
    std::string name;
    double      bid;
    double      ask;
    double      minVolume;    // Minimum lot size (typically 0.01)
    double      maxVolume;    // Maximum lot size (typically 100.0)
    double      volumeStep;   // Lot step (typically 0.01)
    int         digits;       // Price decimal places (5 for EURUSD)
    bool        tradeAllowed;
};

/// Account information returned by MT5 UserAccountGet()
struct AccountInfo {
    int         login;
    double      balance;
    double      equity;
    double      freeMargin;
    double      marginLevel;  // percentage
    std::string currency;
};

/// Abstract interface mirroring the MT5 Manager API.
///
/// In production, this would wrap the real IMTManagerAPI from MetaQuotes SDK.
/// For this demo, MockMTAPI provides simulated broker behavior.
///
/// Key MT5 Manager API methods mapped:
///   connect()         -> IMTManagerAPI::Connect()
///   disconnect()      -> IMTManagerAPI::Disconnect()
///   getSymbolInfo()   -> IMTManagerAPI::SymbolGet() + SymbolInfoGet()
///   getAccountInfo()  -> IMTManagerAPI::UserAccountGet()
///   executeTrade()    -> IMTManagerAPI::DealerSend()
///   getTicketInfo()   -> IMTManagerAPI::DealGet()
///   getSymbols()      -> IMTManagerAPI::SymbolNext() iteration
class IMTBrokerAPI {
public:
    virtual ~IMTBrokerAPI() = default;

    /// Connect to MT5 server (IMTManagerAPI::Connect)
    virtual bool connect(const std::string& server, int login, const std::string& password) = 0;

    /// Disconnect from MT5 server (IMTManagerAPI::Disconnect)
    virtual void disconnect() = 0;

    /// Check if connected
    virtual bool isConnected() const = 0;

    /// Get symbol info including current prices (SymbolGet + SymbolInfoGet)
    virtual std::optional<SymbolInfo> getSymbolInfo(const std::string& symbol) = 0;

    /// Get account balance/margin info (UserAccountGet)
    virtual std::optional<AccountInfo> getAccountInfo(int login) = 0;

    /// Execute a trade via dealer request (DealerSend)
    /// This is the primary trade execution method. DealerSend() is used instead of
    /// direct deal creation because it passes through ALL server-side validations:
    /// margin check, symbol trade limits, session filters, price validation.
    virtual TradeResult executeTrade(const TradeRequest& request) = 0;

    /// Get deal info by ticket (DealGet)
    virtual std::optional<TradeResult> getTicketInfo(const std::string& ticketId) = 0;

    /// Get list of available symbols (SymbolNext iteration)
    virtual std::vector<std::string> getSymbols() = 0;
};
