# 300-Step Execution Plan: WebSockets and SSE Integration

This document outlines the exhaustive 300-step execution plan to implement WebSockets (WS) and Server-Sent Events (SSE) into `c-abstract-http`. It rigorously adheres to the architectural constraints: strict C89 purity, robust `int` error propagation, zero-bloat Windows headers, MSVC safe CRT guards, and full support across all 6 computation modalities.

## Phase 1: Build System & Project Configuration
- [x] 1. Add `C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS` flag to root `CMakeLists.txt`.
- [x] 2. Add `C_ABSTRACT_HTTP_ENABLE_SSE` flag to root `CMakeLists.txt`.
- [x] 3. Create CMake configuration block to isolate WS source files when disabled.
- [x] 4. Create CMake configuration block to isolate SSE source files when disabled.
- [x] 5. Add `test_websocket` executable target in `tests/CMakeLists.txt`.
- [x] 6. Add `test_sse` executable target in `tests/CMakeLists.txt`.
- [x] 7. Link necessary underlying crypto targets for test executables.
- [x] 8. Add `-std=c89` and `-pedantic` to CMake WS/SSE test targets to enforce strict standard.
- [x] 9. Add `-Werror` (or `/WX`) to CMake to ensure zero warnings in new modules.
- [x] 10. Configure CMake to generate `src/ws_config.h.in` for compiled-in limits.
- [x] 11. Configure CMake to generate `src/sse_config.h.in` for compiled-in limits.
- [x] 12. Add `fuzz_ws` target to CMake conditionally enabled by `C_ABSTRACT_HTTP_ENABLE_FUZZING`.
- [x] 13. Add `fuzz_sse` target to CMake conditionally enabled by `C_ABSTRACT_HTTP_ENABLE_FUZZING`.
- [x] 14. Configure CI script `.github/workflows/ci.yml` to build with `C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS=ON`.
- [x] 15. Configure CI script `.github/workflows/ci.yml` to build with `C_ABSTRACT_HTTP_ENABLE_SSE=ON`.

## Phase 2: Header Architecture & Platform Guards
- [x] 16. Create public header `include/c_abstract_http/http_ws.h`.
- [x] 17. Create public header `include/c_abstract_http/http_sse.h`.
- [x] 18. Create private header `src/ws_internal.h`.
- [x] 19. Create private header `src/sse_internal.h`.
- [x] 20. Create private header `src/crypto_utils.h`.
- [x] 21. Apply strict C89 include guards (`#ifndef C_ABSTRACT_HTTP_WS_H`, etc.) to all new headers.
- [x] 22. Add `extern "C" {` guards with `#ifdef __cplusplus` for C++ consumers in public headers.
- [x] 23. Verify `http_ws.h` does not include `<windows.h>` under `_WIN32` (use `<windef.h>`).
- [x] 24. Verify `http_sse.h` does not include `<windows.h>` under `_WIN32` (use `<windef.h>`).
- [x] 25. Define MSVC CRT macros (`SPRINTF_S`, `STRCPY_S`) mapped to `sprintf_s`/`strcpy_s` if `_MSC_VER` is defined.
- [x] 26. Define fallback macros for non-MSVC compilers (`sprintf`, `strcpy`).
- [x] 27. Ensure all local variables in new headers/sources are declared at the top of their blocks (C89 rule).
- [x] 28. Strip all `//` style comments, replace exclusively with `/* */` (C89 rule).
- [x] 29. Include existing `http_types.h` in `http_ws.h` for base structures.
- [x] 30. Include existing `http_types.h` in `http_sse.h` for base structures.

## Phase 3: Core Types & Error Codes
- [x] 31. Define `enum c_abstract_http_ws_opcode` (CONTINUATION=0x0, TEXT=0x1, BINARY=0x2, CLOSE=0x8, PING=0x9, PONG=0xA).
- [x] 32. Define `enum c_abstract_http_ws_state` (CONNECTING, OPEN, CLOSING, CLOSED).
- [x] 33. Define `enum c_abstract_http_sse_state` (CONNECTING, CONNECTED, RECONNECTING, CLOSED).
- [x] 34. Define `struct c_abstract_http_ws_event` (opcode, payload pointer, payload length, is_fin).
- [x] 35. Define `struct c_abstract_http_sse_event` (id pointer, event pointer, data pointer, data length).
- [x] 36. Define `struct c_abstract_http_ws_config` (custom headers, subprotocols, masking preference).
- [x] 37. Define `struct c_abstract_http_sse_config` (last_event_id, retry_timeout_ms).
- [x] 38. Extend `struct c_abstract_http_request_t` with `void* ws_ctx` opaque pointer.
- [x] 39. Extend `struct c_abstract_http_request_t` with `void* sse_ctx` opaque pointer.
- [x] 40. Define `typedef int (*c_abstract_http_ws_on_message)(...)`.
- [x] 41. Define `typedef int (*c_abstract_http_ws_on_error)(...)`.
- [x] 42. Define `typedef int (*c_abstract_http_ws_on_close)(...)`.
- [x] 43. Define `typedef int (*c_abstract_http_sse_on_event)(...)`.
- [x] 44. Define `typedef int (*c_abstract_http_sse_on_error)(...)`.
- [x] 45. Define `typedef int (*c_abstract_http_sse_on_close)(...)`.
- [x] 46. Document error code `C_ABSTRACT_HTTP_ERR_WS_FRAMING` (-1001).
- [x] 47. Document error code `C_ABSTRACT_HTTP_ERR_WS_HANDSHAKE` (-1002).
- [x] 48. Document error code `C_ABSTRACT_HTTP_ERR_SSE_PARSE` (-1003).
- [x] 49. Document error code `C_ABSTRACT_HTTP_ERR_SSE_OOM` (-1004).
- [x] 50. Document error code `C_ABSTRACT_HTTP_ERR_WS_CLOSED` (-1005).

## Phase 4: Cryptography Utils (SHA-1 & Base64)
- [x] 51. Create `src/crypto_utils.c`.
- [x] 52. Define `struct sha1_ctx` containing state, count, and buffer arrays.
- [x] 53. Implement `int sha1_init(struct sha1_ctx* ctx)` returning 0 on success.
- [x] 54. Implement internal `sha1_transform(uint32_t state[5], const uint8_t buffer[64])`.
- [x] 55. Implement `int sha1_update(struct sha1_ctx* ctx, const unsigned char* data, size_t len)`.
- [x] 56. Implement `int sha1_final(struct sha1_ctx* ctx, unsigned char out_hash[20])`.
- [x] 57. Define Base64 encoding table (`ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/`).
- [x] 58. Implement `int base64_encode(const unsigned char* in, size_t len, char** out_str, size_t* out_len)`.
- [x] 59. Ensure `base64_encode` allocates output securely and returns `ENOMEM` on failure.
- [x] 60. Define Base64 decoding lookup table.
- [x] 61. Implement `int base64_decode(const char* in, size_t len, unsigned char** out_data, size_t* out_len)`.
- [x] 62. Ensure `base64_decode` handles padding characters (`=`) correctly.
- [x] 63. Write unit test `test_sha1_empty_string()` in `tests/test_crypto.c`.
- [x] 64. Write unit test `test_sha1_fox_string()` in `tests/test_crypto.c`.
- [x] 65. Write unit test `test_base64_encode_basic()` in `tests/test_crypto.c`.
- [x] 66. Write unit test `test_base64_encode_padded()` in `tests/test_crypto.c`.
- [x] 67. Write unit test `test_base64_decode_basic()` in `tests/test_crypto.c`.
- [x] 68. Write unit test `test_base64_decode_padded()` in `tests/test_crypto.c`.

## Phase 5: WebSocket Handshake & Security
- [x] 69. Implement `int ws_generate_key(char out_key[25])`.
- [x] 70. Fallback to `rand()` or `CryptGenRandom` (Windows) for WS key bytes if no crypto backend is linked.
- [x] 71. Validate `ws_generate_key` outputs exactly 24 characters + null terminator.
- [x] 72. Implement `int ws_sign_key(const char* client_key, char out_accept[29])`.
- [x] 73. Concatenate client key with standard RFC GUID `258EAFA5-E914-47DA-95CA-C5AB0DC85B11`.
- [x] 74. Route concatenated string through `sha1_update`.
- [x] 75. Route SHA-1 output through `base64_encode`.
- [x] 76. Use `STRCPY_S` carefully when copying the resulting Base64 string to `out_accept`.
- [x] 77. Implement `int ws_verify_accept(const char* client_key, const char* server_accept)`.
- [x] 78. Compare generated signature with `server_accept` using constant-time `memcmp`.
- [x] 79. Write unit test `test_ws_generate_key_length()`.
- [x] 80. Write unit test `test_ws_sign_key_rfc_example()` ensuring exact match with RFC 6455 test vector.
- [x] 81. Write unit test `test_ws_verify_accept_success()`.
- [x] 82. Write unit test `test_ws_verify_accept_failure()`.
- [x] 83. Define `int c_abstract_http_ws_init(struct c_abstract_http_request_t* req, ...)`.
- [x] 84. In `ws_init`, append `Upgrade: websocket` to request headers.
- [x] 85. In `ws_init`, append `Connection: Upgrade` to request headers.
- [x] 86. In `ws_init`, append generated `Sec-WebSocket-Key` to request headers.
- [x] 87. In `ws_init`, append `Sec-WebSocket-Version: 13` to request headers.
- [x] 88. In `ws_init`, optionally append `Sec-WebSocket-Protocol` if provided in config.

## Phase 6: WebSocket Framing Engine
- [x] 89. Define `struct ws_frame_header` for internal tracking.
- [x] 90. Implement portable 16-bit byte-swap `uint16_t ws_htons(uint16_t hostshort)`.
- [x] 91. Implement portable 64-bit byte-swap `uint64_t ws_htonll(uint64_t hostqword)`.
- [x] 92. Implement portable 16-bit byte-swap `uint16_t ws_ntohs(uint16_t netshort)`.
- [x] 93. Implement portable 64-bit byte-swap `uint64_t ws_ntohll(uint64_t netqword)`.
- [x] 94. Implement `int ws_generate_mask_key(unsigned char out_key[4])`.
- [x] 95. Implement `int ws_apply_mask(unsigned char* payload, size_t len, const unsigned char mask_key[4])`.
- [x] 96. Implement `int ws_pack_header_small(...)` for payloads <= 125 bytes.
- [x] 97. Implement `int ws_pack_header_medium(...)` for payloads 126 - 65535 bytes (16-bit length).
- [x] 98. Implement `int ws_pack_header_large(...)` for payloads >= 65536 bytes (64-bit length).
- [x] 99. Write unit test `test_ws_pack_header_small()`.
- [x] 100. Write unit test `test_ws_pack_header_medium()`.
- [x] 101. Write unit test `test_ws_pack_header_large()`.
- [x] 102. Write unit test `test_ws_apply_mask_symmetry()` (masking twice should yield original).
- [x] 103. Define `struct ws_parser_ctx` holding dynamic state, payload buffer, and offset indices.
- [x] 104. Implement parser state machine `enum ws_parser_state` (READ_OPCODE, READ_LEN, READ_EXT_LEN, READ_MASK, READ_PAYLOAD).
- [x] 105. Implement `int ws_parser_feed(struct ws_parser_ctx* ctx, const unsigned char* chunk, size_t len)`.
- [x] 106. In `ws_parser_feed`, implement State 1: Parse FIN bit and Opcode.
- [x] 107. In `ws_parser_feed`, fail with `EINVAL` if RSV1, RSV2, or RSV3 bits are set (extensions not supported).
- [x] 108. In `ws_parser_feed`, implement State 2: Parse Mask flag and base payload length (0-127).
- [x] 109. In `ws_parser_feed`, implement State 3: Read 2 or 8 bytes if extended length is indicated.
- [x] 110. In `ws_parser_feed`, fail with `EMSGSIZE` if 64-bit length exceeds predefined maximum internal memory.
- [x] 111. In `ws_parser_feed`, implement State 4: Read 4-byte masking key if Mask flag is 1.
- [x] 112. In `ws_parser_feed`, implement State 5: Read raw payload bytes until length is met.
- [x] 113. Implement on-the-fly unmasking within State 5 to save buffer allocations.
- [x] 114. Implement PING frame handler: immediately queue a PONG frame with identical payload.
- [x] 115. Implement PONG frame handler: invoke an optional user heartbeat callback, discard payload.
- [x] 116. Implement CLOSE frame handler: extract 16-bit status code from first 2 bytes of payload.
- [x] 117. Fallback to status 1005 (No Status Rcvd) if CLOSE frame has no payload.
- [x] 118. Implement Fragmentation handler: allocate secondary `reassembly_buffer`.
- [x] 119. On receiving FIN=0, append to `reassembly_buffer` and do not trigger user callback.
- [x] 120. Fail with `EPROTO` if control frames (PING, PONG, CLOSE) are fragmented (RFC violation).
- [x] 121. Fail with `EPROTO` if a data frame is sent before previous fragmented message concludes.
- [x] 122. On receiving CONTINUATION frame with FIN=1, append final chunk and trigger `on_message` callback.
- [x] 123. Ensure `reassembly_buffer` is properly freed upon stream closure or error.
- [x] 124. Write unit test `test_ws_parser_single_frame()`.
- [x] 125. Write unit test `test_ws_parser_chunked_delivery()` (feeding 1 byte at a time to parser).
- [x] 126. Write unit test `test_ws_parser_fragmented_message()`.
- [x] 127. Write unit test `test_ws_parser_reject_fragmented_control()`.
- [x] 128. Write unit test `test_ws_parser_auto_pong()`.
- [x] 129. Implement `int c_abstract_http_ws_send(...)` which allocates buffer, packs header, packs payload, enqueues to transport.
- [x] 130. Implement `int c_abstract_http_ws_close(...)` which sends a CLOSE frame and transitions state.

## Phase 7: Server-Sent Events (SSE) Parser
- [x] 131. Define `struct sse_parser_ctx` holding raw buffer, current offset, and event state.
- [x] 132. Implement `int sse_parser_init(struct sse_parser_ctx* ctx, const struct c_abstract_http_sse_config* config)`.
- [x] 133. Set default event type to `"message"` on init.
- [x] 134. Implement `int sse_parser_feed(struct sse_parser_ctx* ctx, const char* chunk, size_t len)`.
- [x] 135. In `sse_parser_feed`, append chunk to internal line buffer.
- [x] 136. Return `ENOMEM` if line buffer exceeds maximum allowed line size (prevents memory exhaustion).
- [x] 137. Implement `char* sse_extract_line(struct sse_parser_ctx* ctx)` looking for `\r\n`, `\n`, or `\r`.
- [x] 138. Parse line: if starts with `:`, ignore line (comment).
- [x] 139. Parse line: split at first `:` into `field` and `value`.
- [x] 140. Strip leading single space from `value` (if present) per SSE spec.
- [x] 141. Handle `field="event"`: strdup `value` into `ctx->current_event`.
- [x] 142. Handle `field="data"`: append `value` + `\n` to `ctx->current_data`.
- [x] 143. Handle `field="id"`: strdup `value` into `ctx->last_event_id`.
- [x] 144. Handle `field="retry"`: parse `value` as integer, update `ctx->retry_ms`.
- [x] 145. Handle empty line: signifies end of event block.
- [x] 146. On empty line, if `ctx->current_data` is not empty, trigger `on_event` callback.
- [x] 147. After callback returns, free `current_data` and reset `current_event` to `"message"`.
- [x] 148. Maintain `last_event_id` and `retry_ms` across empty lines.
- [x] 149. Implement `int sse_parser_destroy(struct sse_parser_ctx* ctx)` to prevent leaks.
- [x] 150. Write unit test `test_sse_parse_basic_data()`.
- [x] 151. Write unit test `test_sse_parse_multi_line_data()`.
- [x] 152. Write unit test `test_sse_parse_event_type()`.
- [x] 153. Write unit test `test_sse_parse_ignore_comments()`.
- [x] 154. Write unit test `test_sse_parse_id_and_retry()`.
- [x] 155. Write unit test `test_sse_chunked_delivery()` feeding chunks bisecting a `\r\n`.

## Phase 8: Modality 1 - SYNC Implementation
- [x] 156. Enforce thread-local execution validation for `MODALITY_SYNC`.
- [x] 157. Ensure raw TCP socket uses standard blocking mode (`fcntl` untouched).
- [x] 158. Implement `int ws_sync_handshake(struct c_abstract_http_request_t* req)`.
- [x] 159. Perform blocking `send()` of WS Upgrade HTTP request.
- [x] 160. Perform blocking `recv()` of HTTP 101 Switching Protocols response.
- [x] 161. Parse HTTP 101 headers synchronously to validate `Sec-WebSocket-Accept`.
- [x] 162. Implement `int ws_sync_read_loop(struct c_abstract_http_request_t* req)`.
- [x] 163. Create `while(1)` loop issuing blocking `recv()`, passing buffer to `ws_parser_feed`.
- [x] 164. Implement `int ws_sync_send(...)` using blocking `send()` until all bytes written.
- [x] 165. Apply `setsockopt(SO_RCVTIMEO)` to implement WS ping timeouts in sync mode.
- [x] 166. Handle `WSAETIMEDOUT` / `EAGAIN` correctly if timeout triggers without data.
- [x] 167. Implement `int sse_sync_request(struct c_abstract_http_request_t* req)`.
- [x] 168. Perform blocking `send()` of HTTP GET with `Accept: text/event-stream`.
- [x] 169. Perform blocking `recv()` checking for HTTP 200 OK.
- [x] 170. Implement `int sse_sync_read_loop(struct c_abstract_http_request_t* req)`.
- [x] 171. Create `while(1)` loop issuing blocking `recv()`, passing buffer to `sse_parser_feed`.
- [x] 172. Implement reconnect loop: on connection drop, block and reconnect supplying `Last-Event-ID`.
- [x] 173. Provide thread-safe boolean flag to gracefully exit sync loops from another thread.
- [x] 174. Write integration test `test_modality_sync_ws_echo()`.
- [x] 175. Write integration test `test_modality_sync_sse_stream()`.

## Phase 9: Modality 2 - ASYNC Implementation
- [x] 176. Enforce integration with `event_loop.c` for `MODALITY_ASYNC`.
- [x] 177. Configure raw socket with `fcntl(O_NONBLOCK)` or `ioctlsocket(FIONBIO)`.
- [x] 178. Implement WS handshake as an asynchronous state machine within the event loop.
- [x] 179. Register socket FD into system multiplexer (`epoll`/`kqueue`/`select`).
- [x] 180. Define `struct ws_async_write_queue` for queuing outgoing frames.
- [x] 181. Implement `int ws_async_send(...)`: append frame to `ws_async_write_queue`.
- [x] 182. On `EPOLLOUT` / writable event, drain `ws_async_write_queue` using non-blocking `send()`.
- [x] 183. Handle `EAGAIN` / `EWOULDBLOCK` by pausing writes and maintaining queue offset.
- [x] 184. On `EPOLLIN` / readable event, drain socket into buffer and pass to `ws_parser_feed`.
- [x] 185. Map `c_abstract_http_ws_on_message` callbacks to trigger directly from the read event loop step.
- [x] 186. Manage libuv integration: Map read/write loops to `uv_read_start` and `uv_write`.
- [x] 187. Manage libevent integration: Map read/write loops to `bufferevent` callbacks.
- [x] 188. Ensure `ws_async_write_queue` memory is safely freed on unexpected socket closure.
- [x] 189. Implement SSE async read chunking on `EPOLLIN` tied to `sse_parser_feed`.
- [x] 190. Implement SSE async reconnect logic using an event loop timer callback (`uv_timer_start`).
- [x] 191. WinHTTP Backend: Map async operations to `WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE`.
- [x] 192. WinHTTP Backend: Use native `WinHttpWebSocketReceive` API if OS supports it.
- [x] 193. libcurl Backend: Use `curl_multi_perform` for SSE chunked streaming.
- [x] 194. libcurl Backend: Handle WS via raw curl socket extraction (`CURLINFO_ACTIVESOCKET`).
- [x] 195. Write integration test `test_modality_async_ws_echo()`.
- [x] 196. Write integration test `test_modality_async_sse_stream()`.
- [x] 197. Write integration test `test_modality_async_eagain_handling()`.
- [x] 198. Write integration test `test_modality_async_queue_drain()`.
- [x] 199. Write integration test `test_modality_async_reconnect_timer()`.
- [x] 200. Ensure error propagation (`ECONNRESET`) invokes async `on_error` callbacks correctly.

## Phase 10: Modality 3 - THREAD POOL Implementation
- [x] 201. Define `ws_worker_routine` function to run in background thread.
- [x] 202. Define `sse_worker_routine` function to run in background thread.
- [x] 203. Create thread-safe queue for dispatching WS user callbacks back to the main thread.
- [x] 204. Protect `ws_async_write_queue` with a C89 compatible mutex (`pthread_mutex_t` / `CRITICAL_SECTION`).
- [x] 205. Implement cross-thread wakeup using `eventfd` (Linux) or self-pipe trick (POSIX/Windows).
- [x] 206. When `ws_send` is called from main thread, enqueue to write buffer, then trigger wakeup pipe.
- [x] 207. Worker thread `select`/`poll` monitors both the TCP socket and the wakeup pipe.
- [x] 208. Safely process callbacks in main thread by draining the callback queue during application tick.
- [x] 209. Protect SSE parser state context with mutexes if accessed across threads.
- [x] 210. Provide safe shutdown mechanism: set `exit_flag`, wake worker thread, and call `pthread_join`.
- [x] 211. Write integration test `test_modality_thread_pool_ws_concurrency()` (multiple threads sending).
- [x] 212. Write integration test `test_modality_thread_pool_sse_100_streams()`.
- [x] 213. Validate zero race conditions using ThreadSanitizer (TSan) in CI pipeline.
- [x] 214. Ensure `ENOMEM` in thread pool gracefully reports back to main thread via callback.
- [x] 215. Ensure `CRITICAL_SECTION` is correctly initialized and deleted on Windows.

## Phase 11: Modality 4 - MULTIPROCESS Implementation
- [x] 216. Implement isolated parser initialization in a forked child process.
- [x] 217. Establish bidirectional IPC channel (UNIX Domain Socket or Named Pipe) between parent/child.
- [x] 218. Parent Process: Implement `ws_send` to serialize WS payload into custom IPC struct.
- [x] 219. Parent Process: Send serialized IPC message to child process over pipe.
- [x] 220. Child Process: Deserialize IPC message, pass to raw network TCP socket.
- [x] 221. Child Process: Read TCP socket, parse WS frames, serialize frames into IPC struct.
- [x] 222. Parent Process: Read IPC pipe, deserialize WS frame, trigger user `on_message` callback.
- [x] 223. Implement Shared Memory mapping (`mmap`/`CreateFileMapping`) for WS payloads > 64KB.
- [x] 224. Child Process (SSE): Stream parsed SSE events over IPC to parent.
- [x] 225. Implement `SIGCHLD` handler to detect child process crashes.
- [x] 226. Auto-restart SSE child process on crash, trigger Reconnect state with `Last-Event-ID`.
- [x] 227. Pass underlying TLS socket FD from parent to child using `SCM_RIGHTS` (Linux/BSD).
- [x] 228. Duplicate underlying socket Handle using `DuplicateHandle` (Windows).
- [x] 229. Write integration test `test_modality_multiprocess_ws()`.
- [x] 230. Guarantee zombie process prevention using `waitpid(WNOHANG)` in parent cleanup.

## Phase 12: Modality 5 - GREENTHREAD Implementation
- [x] 231. Wrap basic `recv()` call with a `coroutine_yield()` check.
- [x] 232. If `recv()` returns `EAGAIN`, yield execution context back to scheduler.
- [x] 233. Wrap basic `send()` call with a `coroutine_yield()` check for partial writes.
- [x] 234. In WS logic, code operates sequentially as if Sync, but yields dynamically.
- [x] 235. In SSE logic, `sse_extract_line` yields inside if buffer is exhausted.
- [x] 236. Ensure all WS/SSE state variables are preserved across context switches.
- [x] 237. Configure user-space stack size specifically for WS parser (min 16KB).
- [x] 238. Configure user-space stack size specifically for SSE parser (min 8KB).
- [x] 239. Ensure strict C89 compatibility in local variable declarations to avoid stack pointer corruption.
- [x] 240. Map coroutine exit to WS connection close frame dispatch.
- [x] 241. Map coroutine cancellation to immediate TCP socket teardown.
- [x] 242. Write integration test `test_modality_greenthread_ws_1000_coroutines()`.
- [x] 243. Write integration test `test_modality_greenthread_sse_yield_behavior()`.
- [x] 244. Verify coroutine stack memory is completely freed upon WS closure.
- [x] 245. Run Valgrind with `--tool=memcheck` targeting greenthread stack swapping.

## Phase 13: Modality 6 - MESSAGE PASSING (Actor Model)
- [x] 246. Register `ACTOR_WS_CLIENT` and `ACTOR_SSE_LISTENER` identifiers in `actor.c`.
- [x] 247. Define message types `MSG_WS_CONNECT`, `MSG_WS_SEND`, `MSG_WS_RECEIVE`, `MSG_WS_CLOSE`.
- [x] 248. Define message types `MSG_SSE_CONNECT`, `MSG_SSE_EVENT`.
- [x] 249. Implement Actor mailbox for routing incoming WS frames to recipient actors.
- [x] 250. Implement Actor mailbox for receiving `MSG_WS_SEND` commands to push data.
- [x] 251. Ensure WS/SSE Actors run without external mutexes, relying strictly on message queues.
- [x] 252. Emscripten Backend: Bridge Actor messages to Web Workers using `postMessage`.
- [x] 253. Emscripten WS: Wrap browser's native `WebSocket` API using `EM_ASM` macro.
- [x] 254. Emscripten SSE: Wrap browser's native `EventSource` API using `EM_ASM` macro.
- [x] 255. Map JS `onmessage` and `onclose` events back to C actor mailbox messages.
- [x] 256. Handle JS string memory allocation gracefully returning to C environment.
- [x] 257. Write integration test `test_modality_actor_ws_ping_pong()`.
- [x] 258. Write integration test `test_modality_actor_sse_broadcast()`.
- [x] 259. Write specific Emscripten test suite executed via Node.js runtime.
- [x] 260. Validate zero memory leaks during Actor destruction.

## Phase 14: Documentation & Examples
- [x] 261. Update `ARCHITECTURE.md` to explain the WS framing engine design.
- [x] 262. Update `ARCHITECTURE.md` to explain the SSE parser buffering strategy.
- [x] 263. Add Doxygen `/** */` comments to all public functions in `http_ws.h`.
- [x] 264. Add Doxygen `/** */` comments to all public functions in `http_sse.h`.
- [x] 265. Document memory ownership explicitly (e.g., "payload pointer is only valid during callback").
- [x] 266. Document MSVC string macro safe-guards used internally.
- [x] 267. Create usage example `examples/ws_sync_echo.c`.
- [x] 268. Create usage example `examples/ws_async_chat.c`.
- [x] 269. Create usage example `examples/sse_sync_listener.c`.
- [x] 270. Create usage example `examples/sse_async_reconnect.c`.
- [x] 271. Create usage example `examples/ws_actor_model.c`.
- [x] 272. Update `USAGE.md` with a Modality Matrix showing WS/SSE support.
- [x] 273. Update `README.md` features list to include WebSockets and Server-Sent Events.
- [x] 274. Update `README.md` to mention 1st/2nd party drop-in crypto dependencies.
- [x] 275. Ensure code blocks in README use valid C89 syntax.

## Phase 15: Security, Fuzzing & Static Analysis
- [x] 276. Implement `tests/fuzz_ws_parser.c` hooking libFuzzer into `ws_parser_feed`.
- [x] 277. Implement `tests/fuzz_sse_parser.c` hooking libFuzzer into `sse_parser_feed`.
- [x] 278. Run `fuzz_ws_parser` for 10 million iterations to verify zero out-of-bounds reads.
- [x] 279. Run `fuzz_sse_parser` for 10 million iterations to verify zero buffer overflows.
- [x] 280. Run `/analyze` compiler flag on MSVC to trigger static analysis bounds checking.
- [x] 281. Run `scan-build` (Clang Static Analyzer) over the new WS/SSE modules.
- [x] 282. Fix any warnings related to implicit casting or sign-conversion in bitwise operations.
- [x] 283. Audit fragmentation reassembly limit to guarantee DoS prevention (Max 2MB).
- [x] 284. Audit SSE line buffer limit to guarantee DoS prevention (Max 32KB).
- [x] 285. Audit WS Masking implementation for constant-time predictable branches.

## Phase 16: Unit Tests & Code Coverage
- [x] 286. Extend `cdd_test_helpers/mock_server.c` to accept HTTP `Upgrade: websocket`.
- [x] 287. Configure mock server to emit valid WS 101 Handshake responses.
- [x] 288. Configure mock server to provide a continuous SSE streaming endpoint (`/events`).
- [x] 289. Configure mock server to deliberately inject malformed WS frames to test error handling.
- [x] 290. Configure mock server to drop TCP connection mid-SSE stream to test reconnection.
- [x] 291. Run complete test suite natively on Ubuntu 20.04 (GCC).
- [x] 292. Run complete test suite natively on macOS 14 (Clang).
- [x] 293. Run complete test suite natively on Windows 11 MSVC 2022 (Shared CRT).
- [x] 294. Run complete test suite natively on Windows 11 MSVC 2022 (Static CRT).
- [x] 295. Execute `ctest` hooked into `gcov`/`lcov` for line coverage analysis.
- [x] 296. Assert script fails if `http_ws.c` line coverage is below 100%.
- [x] 297. Assert script fails if `http_sse.c` line coverage is below 100%.
- [x] 298. Execute `ctest` hooked into `gcov` for branch coverage analysis.
- [x] 299. Ensure every `int` error return path (-1001 to -1005) is hit by at least one test.
- [x] 300. Finalize PR, verify CI green checkmarks across all modalities and platforms.
