# MT4/MT5 Deal Processor - Task Breakdown

## Position: C++ + MetaTrader Integration Developer @ Hentec Trading

## Status: Self-contained demo (no credentials needed)

---

## Architecture Overview

```
Client1  Client2  ClientN
   |        |        |
   v        v        v
 ┌─────────────────────────┐
 │  Thread-Safe Queue       │  std::queue + std::mutex + std::condition_variable
 └────────────┬────────────┘
              v
 ┌─────────────────────────┐
 │  Deal Processor          │  Main coordinator
 │  (Worker Thread Pool)    │  std::thread pool (N workers)
 └────────────┬────────────┘
              v
 ┌─────────────────────────┐
 │  MT API Adapter          │  IMTBrokerAPI interface (mocked for demo)
 └────────────┬────────────┘
              v
 ┌─────────────────────────┐
 │  Logger + Result Tracker │  Thread-safe logging, request ID -> MT ticket mapping
 └─────────────────────────┘
```

---

## Tasks

### Phase 1: Project Setup

- [ ] **1.1** Create CMakeLists.txt / Makefile for C++17 build
- [ ] **1.2** Set up directory structure:
  ```
  src/
    main.cpp
    models/TradeRequest.h
    models/TradeResult.h
    queue/ThreadSafeQueue.h
    processor/DealProcessor.h
    processor/DealProcessor.cpp
    mt_api/IMTBrokerAPI.h
    mt_api/MockMTAPI.h
    mt_api/MockMTAPI.cpp
    logger/Logger.h
    logger/Logger.cpp
    tracker/ResultTracker.h
    tracker/ResultTracker.cpp
    client/ClientSimulator.h
    client/ClientSimulator.cpp
  ```
- [ ] **1.3** Verify build compiles on macOS / AWS Linux

---

### Phase 2: Core Data Models

- [ ] **2.1** `TradeRequest` struct
  - Client ID
  - Request ID (unique, for dedup)
  - Trade Type (Buy / Sell)
  - Symbol (e.g., EURUSD)
  - Volume (lot size)
  - Optional: Stop Loss / Take Profit
  - Timestamp

- [ ] **2.2** `TradeResult` struct
  - Request ID (maps back to client request)
  - MT Ticket ID (from broker)
  - Status (Success / Rejected / Error)
  - Error message (if any)
  - Execution price
  - Timestamp

---

### Phase 3: Thread-Safe Queue

- [ ] **3.1** Implement `ThreadSafeQueue<T>` using `std::queue`, `std::mutex`, `std::condition_variable`
  - `push()` - enqueue with notification
  - `pop()` - blocking dequeue with wait
  - `tryPop()` - non-blocking dequeue
  - `size()` - current queue depth
  - `shutdown()` - signal all waiting threads to exit

---

### Phase 4: MT API Adapter (Interface + Mock)

- [ ] **4.1** `IMTBrokerAPI` interface (abstract class)
  - `connect()` / `disconnect()`
  - `getSymbolInfo(symbol)` - validate symbol exists
  - `getAccountMargin(login)` - check available margin
  - `executeTrade(request)` - execute buy/sell order
  - `getTicketInfo(ticket)` - query order status
  - Map to real MT5 Manager API methods:
    - `IMTManagerAPI::DealerBalance()` -> margin check
    - `IMTManagerAPI::OrderSend()` -> trade execution
    - `IMTManagerAPI::SymbolGet()` -> symbol validation
    - `IMTManagerAPI::DealGet()` -> ticket lookup

- [ ] **4.2** `MockMTAPI` implementation
  - Simulates realistic MT5 responses
  - Random execution delays (10-100ms)
  - Configurable failure rate (e.g., 5% rejection)
  - Validates symbol against known list (EURUSD, GBPUSD, USDJPY, etc.)
  - Validates volume (min 0.01, max 100.0 lots)
  - Generates mock ticket IDs
  - Simulates connection errors occasionally

---

### Phase 5: Request Validator

- [ ] **5.1** Implement validation layer before MT API execution
  - Symbol validation (is it a known/tradeable symbol?)
  - Volume validation (within min/max lot range?)
  - Margin check (sufficient balance for the trade?)
  - Duplicate request detection (same request ID seen before?)
  - Parameter sanity checks (SL/TP levels make sense?)

---

### Phase 6: Deal Processor (Core Engine)

- [ ] **6.1** `DealProcessor` class
  - Owns the `ThreadSafeQueue`
  - Spawns N worker threads (`std::thread` pool)
  - Each worker: dequeue -> validate -> execute -> track result -> log
  - Graceful shutdown (drain queue, join threads)
  - Thread-safe submission of new requests

- [ ] **6.2** Worker thread loop
  ```
  while (!shutdown) {
      request = queue.pop();       // blocks until available
      validate(request);           // check params
      result = mtAPI.execute();    // call broker API
      tracker.record(result);      // store mapping
      logger.log(result);          // log everything
      notifyClient(result);        // callback/promise
  }
  ```

---

### Phase 7: Result Tracker

- [ ] **7.1** `ResultTracker` - thread-safe map
  - `std::unordered_map<requestId, TradeResult>` with mutex
  - Maps client request ID -> MT ticket ID (bonus requirement)
  - Query results by client ID or request ID
  - Track success/failure counts per client

---

### Phase 8: Logger

- [ ] **8.1** Thread-safe `Logger` class
  - Log to console + file simultaneously
  - Log levels: INFO, WARN, ERROR, DEBUG
  - Timestamps on every entry
  - Format: `[2026-02-26 14:30:15.123] [INFO] [Thread-3] Client-42 BUY EURUSD 1.0 -> Ticket #12345 SUCCESS`
  - Log events:
    - Request received
    - Validation passed/failed
    - Trade submitted to MT API
    - Trade executed (with ticket ID)
    - Trade rejected (with reason)
    - Errors (connection, timeout, etc.)

---

### Phase 9: Client Simulators

- [ ] **9.1** `ClientSimulator` class
  - Each client runs in its own `std::thread`
  - Generates random trade requests (Buy/Sell, various symbols, volumes)
  - Submits requests to the Deal Processor queue
  - Receives results via callback / future
  - Configurable: request rate, number of requests

- [ ] **9.2** Main simulation harness
  - Spawn N client simulators (e.g., 5-10 clients)
  - Each client sends M requests (e.g., 10-50 each)
  - Run concurrently, collect all results
  - Print summary stats at end

---

### Phase 10: Bonus Features (Optional Extensions)

- [ ] **10.1** Request ID -> MT Ticket ID mapping (covered in ResultTracker)
- [ ] **10.2** Retry mechanism for failed trades
  - Configurable max retries (e.g., 3)
  - Exponential backoff between retries
  - Different retry behavior for transient vs permanent errors
- [ ] **10.3** High-frequency burst test
  - Simulate 100+ requests in < 1 second
  - Verify no lost requests
  - Measure throughput and latency

---

### Phase 11: Documentation & Deliverables

- [ ] **11.1** README.md with:
  - Build/run instructions
  - Which MT API methods were used and why
  - Threading model explanation
  - How request processing works (flow diagram)
- [ ] **11.2** Sample log output showing multiple clients being processed
- [ ] **11.3** Build verification (compiles and runs cleanly)

---

## MT5 Manager API Methods Reference

| Method | Purpose | Used For |
|--------|---------|----------|
| `IMTManagerAPI::Connect()` | Connect to MT5 server | Initial connection setup |
| `IMTManagerAPI::Disconnect()` | Disconnect from server | Graceful shutdown |
| `IMTManagerAPI::SymbolGet()` | Get symbol configuration | Validate symbol exists & get specs |
| `IMTManagerAPI::SymbolInfoGet()` | Get real-time symbol info | Get current bid/ask prices |
| `IMTManagerAPI::UserGet()` | Get user/account info | Check account exists |
| `IMTManagerAPI::UserAccountGet()` | Get account balance/margin | Margin validation |
| `IMTManagerAPI::DealerSend()` | Send a dealing request | **Primary trade execution** |
| `IMTManagerAPI::OrderSend()` | Place a pending order | Alternative order placement |
| `IMTManagerAPI::DealGet()` | Get deal by ticket | Verify trade execution |
| `IMTManagerAPI::OrderGet()` | Get order by ticket | Check order status |
| `IMTManagerAPI::PositionGet()` | Get open position | Track positions after execution |
| `IMTManagerAPI::LoggerOut()` | Write to server log | Server-side logging |

### Key API Flow for Trade Execution:
```
1. SymbolGet(symbol)           -> Validate symbol
2. UserAccountGet(login)       -> Check margin
3. SymbolInfoGet(symbol)       -> Get current price
4. DealerSend(trade_request)   -> Execute trade (passes server-side validations)
5. DealGet(ticket)             -> Confirm execution
```

> **Important**: `DealerSend()` is the correct method for dealer/manager-initiated trades
> because it goes through all server-side validations (margin check, symbol limits,
> trade permissions). This is different from direct deal creation which bypasses checks.

---

## Evaluation Criteria Checklist

| Area | What They Want | How We Address It |
|------|---------------|-------------------|
| MT API Knowledge | Correct usage of Manager/Admin API methods | IMTBrokerAPI interface mirrors real MT5 API; README explains method choices |
| Concurrency | Thread-safe processing, no lost requests | ThreadSafeQueue + mutex + condition_variable; worker pool pattern |
| Logging & Tracking | Complete record of requests and executions | Thread-safe Logger + ResultTracker with full audit trail |
| Error Handling | Rejections, invalid params, connection errors, duplicates | Validation layer + retry mechanism + dedup detection |
| Architecture | Clean separation of client handling and MT execution | Layered design: Client -> Queue -> Processor -> Validator -> MT API |

---

## Tech Stack

- **Language**: C++17
- **Threading**: `std::thread`, `std::mutex`, `std::condition_variable`, `std::future`
- **Build**: CMake 3.16+
- **Platform**: macOS (dev) / Linux (AWS)
- **No external dependencies** (pure standard library)

---

## Timeline Estimate

| Phase | Description |
|-------|-------------|
| Phase 1-2 | Project setup + data models |
| Phase 3-5 | Queue + MT API + Validator |
| Phase 6-8 | Deal Processor + Tracker + Logger |
| Phase 9 | Client simulators + main harness |
| Phase 10 | Bonus features (retry, burst test) |
| Phase 11 | Documentation + sample logs |
