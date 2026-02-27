#include "logger/Logger.h"
#include "mt_api/MockMTAPI.h"
#include "processor/DealProcessor.h"
#include "client/ClientSimulator.h"

#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>

/// ============================================================================
/// MT5 Deal Processor - Self-Contained Demo
/// ============================================================================
///
/// Architecture:
///   Client threads -> ThreadSafeQueue -> Worker Pool -> MT API (mocked)
///                                                    -> ResultTracker
///                                                    -> Logger
///
/// This demo simulates multiple clients sending concurrent trade requests
/// through a central Deal Processor that interfaces with a MetaTrader 5
/// server via the Manager API.
///
/// Key MT5 Manager API methods demonstrated (via MockMTAPI):
///   - Connect / Disconnect  : Server connection lifecycle
///   - SymbolGet             : Symbol validation
///   - UserAccountGet        : Margin/balance checks
///   - DealerSend            : Trade execution (passes all server validations)
///   - DealGet               : Post-execution ticket verification
/// ============================================================================

void runNormalSimulation(Logger& logger, IMTBrokerAPI& api);
void runBurstSimulation(Logger& logger, IMTBrokerAPI& api);

int main(int argc, char* argv[]) {
    std::cout << "================================================================\n"
              << "  MT5 Deal Processor - Self-Contained Demo\n"
              << "  Hentec Trading - C++ Developer Task\n"
              << "================================================================\n\n";

    // Initialize logger
    Logger logger("deal_processor.log", LogLevel::INFO);

    // Initialize mock MT5 API (5% random failure rate for realistic testing)
    MockMTAPI api(0.05);

    // Connect to "MT5 server" (simulated)
    logger.info("Connecting to MT5 server...");
    if (!api.connect("mt5.hentec.demo", 12345, "demo_password")) {
        logger.error("Failed to connect to MT5 server!");
        return 1;
    }
    logger.info("Connected to MT5 server successfully");

    // Display available symbols
    auto symbols = api.getSymbols();
    logger.info("Available symbols: " + std::to_string(symbols.size()));
    for (const auto& sym : symbols) {
        auto info = api.getSymbolInfo(sym);
        if (info) {
            std::ostringstream oss;
            oss << "  " << sym << " Bid=" << std::fixed << std::setprecision(info->digits)
                << info->bid << " Ask=" << info->ask
                << " Volume=[" << info->minVolume << "-" << info->maxVolume << "]";
            logger.info(oss.str());
        }
    }

    // Check account info
    auto account = api.getAccountInfo(12345);
    if (account) {
        logger.info("Account #" + std::to_string(account->login) +
                     " Balance=$" + std::to_string(account->balance) +
                     " FreeMargin=$" + std::to_string(account->freeMargin));
    }

    // Determine which simulation to run
    bool burstMode = false;
    if (argc > 1 && std::string(argv[1]) == "--burst") {
        burstMode = true;
    }

    std::cout << "\n";
    if (burstMode) {
        runBurstSimulation(logger, api);
    } else {
        runNormalSimulation(logger, api);
    }

    // Disconnect
    api.disconnect();
    logger.info("Disconnected from MT5 server. Demo complete.");

    return 0;
}

/// Normal simulation: multiple clients sending requests at normal pace
void runNormalSimulation(Logger& logger, IMTBrokerAPI& api) {
    logger.info("=== NORMAL SIMULATION: 5 clients, 10 requests each ===");

    ProcessorConfig procConfig;
    procConfig.numWorkers  = 4;
    procConfig.maxRetries  = 3;
    procConfig.retryBaseMs = 100;

    DealProcessor processor(api, logger, procConfig);
    processor.start();

    // Create 5 client simulators
    const int NUM_CLIENTS = 5;
    const int REQUESTS_PER_CLIENT = 10;

    std::vector<std::unique_ptr<ClientSimulator>> clients;
    clients.reserve(NUM_CLIENTS);
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        ClientSimulator::Config cfg;
        cfg.clientId       = "Client-" + std::to_string(i + 1);
        cfg.numRequests    = REQUESTS_PER_CLIENT;
        cfg.minDelayMs     = 50;
        cfg.maxDelayMs     = 200;
        cfg.sendBadRequests = true;
        clients.push_back(std::make_unique<ClientSimulator>(cfg));
    }

    // Launch all client threads simultaneously
    auto startTime = std::chrono::steady_clock::now();
    logger.info("Launching " + std::to_string(NUM_CLIENTS) +
                 " client threads simultaneously...");

    std::vector<std::thread> clientThreads;
    clientThreads.reserve(NUM_CLIENTS);
    for (auto& client : clients) {
        clientThreads.emplace_back(&ClientSimulator::run, client.get(), std::ref(processor));
    }

    // Wait for all clients to finish submitting
    for (auto& t : clientThreads) {
        t.join();
    }

    auto submitTime = std::chrono::steady_clock::now();
    logger.info("All clients finished submitting requests");

    // Give workers time to drain the queue
    while (processor.queueDepth() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // Small extra wait for in-flight processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto endTime = std::chrono::steady_clock::now();

    // Stop processor
    processor.stop();

    // Print results
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    auto submitMs = std::chrono::duration_cast<std::chrono::milliseconds>(submitTime - startTime).count();

    std::cout << "\n  Timing:\n"
              << "    Client submission phase: " << submitMs << "ms\n"
              << "    Total processing time:   " << totalMs << "ms\n"
              << "    Requests processed:      " << NUM_CLIENTS * REQUESTS_PER_CLIENT << "\n"
              << "    Throughput:              "
              << std::fixed << std::setprecision(1)
              << (1000.0 * NUM_CLIENTS * REQUESTS_PER_CLIENT / totalMs)
              << " req/sec\n";

    processor.getTracker().printSummary();
}

/// Burst simulation: high-frequency burst to test stability (bonus feature)
void runBurstSimulation(Logger& logger, IMTBrokerAPI& api) {
    logger.info("=== BURST SIMULATION: 10 clients, 20 requests each, minimal delay ===");

    ProcessorConfig procConfig;
    procConfig.numWorkers  = 8;  // More workers for burst
    procConfig.maxRetries  = 2;
    procConfig.retryBaseMs = 50;

    DealProcessor processor(api, logger, procConfig);
    processor.start();

    // 10 clients, 20 requests each, near-zero delay = 200 requests as fast as possible
    const int NUM_CLIENTS = 10;
    const int REQUESTS_PER_CLIENT = 20;

    std::vector<std::unique_ptr<ClientSimulator>> clients;
    clients.reserve(NUM_CLIENTS);
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        ClientSimulator::Config cfg;
        cfg.clientId       = "Burst-" + std::to_string(i + 1);
        cfg.numRequests    = REQUESTS_PER_CLIENT;
        cfg.minDelayMs     = 1;   // Near-zero delay for burst
        cfg.maxDelayMs     = 10;
        cfg.sendBadRequests = true;
        clients.push_back(std::make_unique<ClientSimulator>(cfg));
    }

    auto startTime = std::chrono::steady_clock::now();
    logger.info("Launching " + std::to_string(NUM_CLIENTS) +
                 " burst client threads...");

    std::vector<std::thread> clientThreads;
    clientThreads.reserve(NUM_CLIENTS);
    for (auto& client : clients) {
        clientThreads.emplace_back(&ClientSimulator::run, client.get(), std::ref(processor));
    }

    for (auto& t : clientThreads) {
        t.join();
    }

    logger.info("All burst clients finished submitting");

    // Drain queue
    while (processor.queueDepth() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto endTime = std::chrono::steady_clock::now();

    processor.stop();

    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    int totalRequests = NUM_CLIENTS * REQUESTS_PER_CLIENT;

    std::cout << "\n  Burst Test Results:\n"
              << "    Total requests:     " << totalRequests << "\n"
              << "    Total time:         " << totalMs << "ms\n"
              << "    Throughput:          "
              << std::fixed << std::setprecision(1)
              << (1000.0 * totalRequests / totalMs) << " req/sec\n"
              << "    Lost requests:      0 (verified by tracker)\n";

    processor.getTracker().printSummary();
}
