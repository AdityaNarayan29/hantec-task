# MT5 Deal Processor

A multi-threaded central Deal Processor that demonstrates integration with the MetaTrader 5 Manager API. Built in C++17 as a self-contained demo for the Hentec Trading C++ Developer position.

## Build & Run

### Prerequisites

- C++17 compiler (GCC 8+, Clang 10+, Apple Clang 12+)
- CMake 3.16+

### Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

### Run

```bash
# Normal simulation: 5 clients, 10 requests each
./deal_processor

# High-frequency burst test: 10 clients, 20 requests each, minimal delay
./deal_processor --burst
```

Log output is written to both the console and `deal_processor.log`.

---

## Architecture

```
Client-1  Client-2  ...  Client-N        (std::thread per client)
   |         |              |
   v         v              v
┌──────────────────────────────────┐
│       ThreadSafeQueue            │     std::queue + mutex + condition_variable
│  (thread-safe request buffer)    │
└──────────────┬───────────────────┘
               v
┌──────────────────────────────────┐
│         DealProcessor            │     Coordinator
│  ┌────────┐ ┌────────┐ ┌──────┐ │
│  │Worker-0│ │Worker-1│ │Wkr-N │ │     std::thread pool (configurable)
│  └───┬────┘ └───┬────┘ └──┬───┘ │
└──────┼──────────┼─────────┼──────┘
       v          v         v
┌──────────────────────────────────┐
│          Validator               │     Pre-execution checks
│  (symbol, volume, margin, dedup) │
└──────────────┬───────────────────┘
               v
┌──────────────────────────────────┐
│       IMTBrokerAPI (interface)   │     Abstract MT5 Manager API
│         MockMTAPI (impl)         │     Simulated broker behavior
└──────────────┬───────────────────┘
               v
┌──────────────┴───────────────────┐
│  ResultTracker  │     Logger     │     Thread-safe tracking & logging
│  (req→ticket)   │  (file+stdout) │
└─────────────────┴────────────────┘
```

---

## MT5 Manager API Methods Used

The `IMTBrokerAPI` interface maps directly to the MT5 Manager API SDK:

| Interface Method | MT5 Manager API | Purpose |
|---|---|---|
| `connect()` | `IMTManagerAPI::Connect()` | Establish connection to the MT5 server |
| `disconnect()` | `IMTManagerAPI::Disconnect()` | Graceful disconnection |
| `getSymbolInfo()` | `IMTManagerAPI::SymbolGet()` + `SymbolInfoGet()` | Validate symbol exists, get trading specs and live prices |
| `getAccountInfo()` | `IMTManagerAPI::UserAccountGet()` | Check account balance and free margin |
| `executeTrade()` | **`IMTManagerAPI::DealerSend()`** | Execute trade via dealer request |
| `getTicketInfo()` | `IMTManagerAPI::DealGet()` | Verify deal execution, retrieve ticket details |
| `getSymbols()` | `IMTManagerAPI::SymbolNext()` | Enumerate available trading symbols |

### Why DealerSend()?

`DealerSend()` is the correct method for manager/dealer-initiated trades because:

1. It passes through **all server-side validations** (margin check, symbol trade limits, session filters, price validation)
2. It respects **trading hours** and **symbol restrictions**
3. It performs **price verification** against current market
4. It returns a proper **deal ticket** on success

This is different from direct deal creation methods which bypass server-side checks.

### Execution Flow (per request)

```
1. SymbolGet(symbol)           → Validate symbol exists and is tradeable
2. UserAccountGet(login)       → Check sufficient free margin
3. SymbolInfoGet(symbol)       → Get current bid/ask for price validation
4. DealerSend(trade_request)   → Execute trade through all validations
5. DealGet(ticket)             → Confirm execution, get fill details
```

---

## Threading Model

### Producer-Consumer Pattern

The system uses a classic **producer-consumer** pattern with a bounded thread pool:

- **Producers**: N client threads push `TradeRequest` objects into a shared queue
- **Buffer**: `ThreadSafeQueue` (std::queue + mutex + condition_variable)
- **Consumers**: M worker threads pop requests and process them independently

### Synchronization

| Component | Mechanism | Purpose |
|---|---|---|
| `ThreadSafeQueue` | `std::mutex` + `std::condition_variable` | Blocking pop, thread-safe push |
| `Logger` | `std::mutex` | Serialized console/file output |
| `ResultTracker` | `std::mutex` | Thread-safe result storage |
| `Validator` | `std::mutex` | Duplicate request detection set |
| `MockMTAPI` | `std::mutex` (account + RNG) | Simulated margin tracking |

### Shutdown Sequence

1. Client threads finish submitting → join
2. `DealProcessor::stop()` sets `running_ = false`
3. `ThreadSafeQueue::shutdown()` wakes all blocked workers
4. Workers drain remaining items, then exit
5. All worker threads joined → clean shutdown

No requests are lost during shutdown because the queue is drained before workers exit.

---

## Request Processing Flow

```
Client submits request
        │
        v
   ┌─ Enqueue in ThreadSafeQueue ─┐
   │                               │
   v                               │
Worker dequeues request            │  (blocks via condition_variable)
   │                               │
   v                               │
Validator checks:                  │
  ├─ Duplicate request? ──────> DUPLICATE error
  ├─ Valid parameters? ────────> INVALID_PARAMS error
  ├─ Symbol exists? ──────────> INVALID_PARAMS error
  ├─ Volume in range? ────────> INVALID_PARAMS error
  └─ SL/TP valid? ────────────> INVALID_PARAMS error
   │
   v (all checks pass)
MT API executeTrade() (DealerSend)
   │
   ├─ Success ──────────────> Record ticket, log, callback
   │
   ├─ Transient failure ───> Retry with exponential backoff
   │   └─ Retry 1: 100ms delay
   │   └─ Retry 2: 200ms delay
   │   └─ Retry 3: 400ms delay
   │   └─ All failed ──────> RETRY_EXHAUSTED error
   │
   └─ Permanent failure ──> Record error, log, callback
```

---

## Error Handling

| Error Type | Detection | Response |
|---|---|---|
| Duplicate request | Request ID seen before (set lookup) | Immediate rejection, no MT API call |
| Invalid symbol | `SymbolGet()` returns not found | Validation error before execution |
| Invalid volume | Outside min/max or wrong step | Validation error before execution |
| Insufficient margin | `UserAccountGet()` free margin too low | Margin error from MT API |
| Connection timeout | Simulated random failure | Retry with exponential backoff |
| Trade rejection | Server-side rejection via `DealerSend()` | Retry (may be transient) |
| Empty parameters | Null/empty client ID, symbol | Validation error before execution |

---

## Bonus Features Implemented

1. **Request ID → MT Ticket ID mapping**: `ResultTracker` maintains a full map of every client request to its corresponding MT deal ticket. Printed in the summary.

2. **Retry mechanism**: Failed trades with transient errors (connection timeouts, temporary rejections) are retried up to 3 times with **exponential backoff** (100ms → 200ms → 400ms).

3. **High-frequency burst test**: Run with `--burst` flag to simulate 200 requests across 10 clients with near-zero delay. Verifies no lost requests and measures throughput.

---

## Project Structure

```
src/
├── main.cpp                    Entry point + simulation harness
├── models/
│   ├── TradeRequest.h          Trade request data structure
│   └── TradeResult.h           Trade result data structure
├── queue/
│   └── ThreadSafeQueue.h       Lock-based concurrent queue (header-only)
├── processor/
│   ├── DealProcessor.h/cpp     Central processor + worker pool
│   └── Validator.h             Pre-execution validation layer
├── mt_api/
│   ├── IMTBrokerAPI.h          Abstract MT5 Manager API interface
│   └── MockMTAPI.h/cpp         Simulated broker (realistic behavior)
├── logger/
│   └── Logger.h/cpp            Thread-safe dual-output logger
├── tracker/
│   └── ResultTracker.h/cpp     Result storage + statistics
└── client/
    └── ClientSimulator.h/cpp   Multi-threaded client simulation
```

---

## Adapting to Production MT5

To connect to a real MT5 server, implement `IMTBrokerAPI` using the MetaQuotes Manager API SDK:

```cpp
class RealMT5API : public IMTBrokerAPI {
    IMTManagerAPI* manager_;  // MetaQuotes SDK handle
public:
    bool connect(...) override {
        // manager_->Connect(server, login, password);
    }
    TradeResult executeTrade(const TradeRequest& req) override {
        // IMTDealerRequest* dealer = ...;
        // dealer->Action(IMTDealerRequest::ACTION_DEAL);
        // dealer->Symbol(req.symbol.c_str());
        // dealer->Volume(req.volume);
        // manager_->DealerSend(dealer, answer);
    }
};
```

The rest of the system (queue, processor, validator, tracker, logger) remains unchanged.
