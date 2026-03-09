# Modality & Concurrency Architecture Plan

## 1. Tradeoffs and Architectural Modifications

To support sending multiple requests at once and introduce a comprehensive `enum`-driven modality selection for the framework user, the architecture of the library must evolve from a simple request-response model to a flexible runtime environment.

### Computational Paradigms & Tradeoffs

1. **Synchronous (Single-threaded)**
   *   **Pros:** Extremely simple to reason about, trivial debugging, minimal memory overhead.
   *   **Cons:** Blocks the entire application during I/O. Sending multiple HTTP requests happens sequentially, maximizing latency.
2. **Asynchronous (Event Loop / Single-threaded)**
   *   **Pros:** Highly scalable for I/O bound tasks (like Node.js). Thousands of concurrent HTTP requests can be handled on a single thread without context-switching overhead.
   *   **Cons:** Requires state machines, callbacks, or futures/promises in C. Debugging is harder due to fragmented stack traces. CPU-bound tasks will block the event loop.
3. **Multithreading (Native OS Threads)**
   *   **Pros:** Allows true parallelism on multi-core CPUs. Easy to map synchronous HTTP requests to individual threads.
   *   **Cons:** High memory overhead per thread (OS stack). Synchronization (mutexes, semaphores) introduces race conditions and deadlocks. Context switching is expensive compared to event loops.
4. **Multiprocess**
   *   **Pros:** Ultimate isolation and fault tolerance. A crash in one request handler doesn't crash the main process. Bypasses Global Interpreter Locks (if binding to other languages).
   *   **Cons:** Highest memory footprint. Requires complex Inter-Process Communication (IPC) like pipes, shared memory, or message queues to aggregate results.
5. **Greenthreads / Coroutines (User-space Threads)**
   *   **Pros:** Combines the simplicity of synchronous code with the performance of asynchronous I/O. Extremely lightweight context switching.
   *   **Cons:** Highly platform-dependent (e.g., `ucontext`, Assembly). Careful stack size management is required to avoid stack overflows.
6. **Message Passing (Actor Model)**
   *   **Pros:** Inherently thread-safe state management. Decouples the sender from the receiver.
   *   **Cons:** Adds latency to communication. Requires building a robust internal message bus/queue system.

### Architectural Modifications Required

To support this in `c-abstract-http` (and integrate with `c-multiplatform`), we need to introduce a **Runtime Context** that abstracts the chosen execution model. 

1. **`enum ExecutionModality`**: A configuration option passed during library/client initialization.
2. **`HttpMultiClient` API**: A new set of APIs (e.g., `http_client_send_multi`) that accepts an array of requests.
3. **Transport Layer Adaptations**: Existing transports (`libcurl`, `winhttp`, `libsoup`, `libuv`) must be adapted to non-blocking or multi-interfaces (e.g., `curl_multi_perform`, WinHTTP async, or `epoll`/`kqueue` loops).
4. **Future/Promise Abstraction**: Since C lacks native async/await, we will need a `HttpFuture` or robust callback system (`on_success`, `on_progress`, `on_error`) to handle completions across all paradigms transparently.

---

## 2. 50-Step Implementation Plan

### Phase 1: Core API & Definitions
- [x] 1. Define `enum ExecutionModality` containing `MODALITY_SYNC`, `MODALITY_ASYNC`, `MODALITY_THREAD_POOL`, `MODALITY_MULTIPROCESS`, `MODALITY_GREENTHREAD`, `MODALITY_MESSAGE_PASSING`.
- [x] 2. Update `HttpConfig` structure to include `enum ExecutionModality`.
- [x] 3. Create a unified `FrameworkRuntime` or `ModalityContext` structure that acts as the backbone for the chosen paradigm.
- [x] 4. Define a generic `HttpFuture` or `HttpPromise` struct to handle deferred responses.
- [x] 5. Define `http_multi_request` struct as an aggregate for multiple `HttpRequest` objects.

### Phase 2: Asynchronous Event Loop (Node.js style)
- [x] 6. Implement `ModalityEventLoop` context initialization.
- [x] 7. Integrate a platform-agnostic polling mechanism (`epoll` on Linux, `kqueue` on macOS, `IOCP` on Windows, or heavily lean on `libuv` if configured).
- [x] 8. Refactor internal socket handling to support `O_NONBLOCK` mode transparently.
- [x] 9. Implement a callback registry to map socket readiness to the correct `HttpRequest`.
- [x] 10. Implement timeouts using a timer wheel or min-heap within the event loop.
- [x] 11. Create the `http_loop_run()` and `http_loop_stop()` functions to mimic Node's `uv_run`.
- [x] 12. Connect `curl_multi_*` interface for the `http_curl` backend in Async mode.
- [x] 13. Connect WinHTTP asynchronous callbacks for the `http_winhttp` backend.
- [x] 14. Add unit tests for sending 100 concurrent asynchronous requests to a mock server.
- [x] 15. Ensure Async mode safely returns an error if a blocking CPU task is detected (or warn user).

### Phase 3: Multithreading (Thread Pool)
- [x] 16. Implement a cross-platform Thread Pool API (`cdd_thread_pool_init`, `cdd_thread_pool_push`).
- [x] 17. Implement cross-platform Mutexes and Condition Variables.
- [x] 18. Map `MODALITY_THREAD_POOL` to spawn `N` worker threads.
- [x] 19. Create a thread-safe task queue to distribute `HttpRequest` structs to workers.
- [x] 20. Implement thread-safe response aggregation (pushing `HttpResponse` back to the main thread or user callback).
- [x] 21. Handle thread pool shutdown and resource cleanup gracefully.
- [x] 22. Implement thread-local storage (TLS) for thread-safe cookie/session management.
- [x] 23. Connect blocking transport backends (`http_fetch`, standard `curl_easy`) to the Thread Pool workers.
- [x] 24. Write thread-safety stress tests (race condition detection via TSAN).
- [x] 25. Add configuration for min/max thread count in `HttpConfig`.

### Phase 4: Multiprocessing
- [x] 26. Implement cross-platform process spawning (`fork()` / `CreateProcess`).
- [x] 27. Implement an IPC mechanism (e.g., anonymous pipes or shared memory) for passing request/response data.
- [x] 28. Map `MODALITY_MULTIPROCESS` to dispatch workloads to child processes.
- [x] 29. Implement serialization/deserialization of `HttpRequest` and `HttpResponse` for IPC boundaries.
- [x] 30. Handle child process lifecycle (zombie process reaping, SIGCHLD on POSIX).

### Phase 5: Greenthreads / Coroutines
- [x] 31. Research and integrate a minimalistic user-space threading library (e.g., `libco`, `ucontext`, or setjmp/longjmp based).
- [x] 32. Implement a `yield()` function to manually hand over execution inside long I/O operations.
- [x] 33. Hook the greenthread scheduler into the Async Event Loop (so `yield` waits for socket readiness).
- [x] 34. Map `MODALITY_GREENTHREAD` initialization to setup the coroutine environment.
- [x] 35. Write tests verifying stack boundaries and coroutine switching under heavy I/O.

### Phase 6: Message Passing (Actor Model)
- [x] 36. Define `MessageBus` and `Actor` structs.
- [x] 37. Implement publish/subscribe (PubSub) mechanics for network events.
- [x] 38. Map `MODALITY_MESSAGE_PASSING` to start an HTTP Actor that listens for "HTTP_SEND" messages.
- [x] 39. Implement the message mailbox and routing logic.
- [x] 40. Write tests verifying decoupled actors can send requests and receive "HTTP_RESPONSE" messages asynchronously.

### Phase 7: Multi-Request API (e.g., File Downloading)
- [x] 41. Design `http_client_send_multi(client, requests, num_requests, callbacks)`.
- [x] 42. Implement automatic batching logic that routes the multi-request array to the configured Modality Engine.
- [x] 43. Add a unified progress callback `on_multi_progress` to track total download completion (e.g., for 5 files at once).
- [x] 44. Implement a fail-fast vs. fail-safe configuration (if one file fails, do we cancel the rest?).
- [x] 45. Add specific examples in `USAGE.md` for downloading multiple files simultaneously.

### Phase 8: Refinement, Testing & Documentation
- [x] 46. Audit memory leaks across all modalities using ASAN/Valgrind.
- [x] 47. Standardize error codes across all computation paradigms.
- [x] 48. Benchmark Async vs. ThreadPool vs. Sync and document performance differences.
- [x] 49. Integrate `MODALITY0.md` concepts deeply into `c-multiplatform/MODALITY.md`.
- [x] 50. Finalize user-facing documentation and await final architecture sign-off.
