# c-abstract-http OAuth2 Implementation Plan 0

To securely communicate with the OAuth2 authorization server, `c-abstract-http` must fulfill these absolute baseline requirements for secure transit and header negotiation:

## 1. Native TLS/HTTPS Support Integration
- **Feature**: A fully integrated, robust secure transport layer abstraction.
- **Why**: The OAuth2 specification strictly requires TLS/HTTPS to prevent credential interception (Man-in-the-Middle attacks) when submitting the password grant.
- **Exhaustive Requirements**:
  - Windows: Native `Schannel` integration to leverage the OS trust store without distributing external `.dll`s.
  - macOS/iOS: Native `SecureTransport` integration.
  - Linux/Android: Abstracted integration for `OpenSSL`, `mbedTLS`, or `BoringSSL`.
  - Strict certificate hostname verification and chain-of-trust validation enabled by default.

## 2. CORS Preflight Handling (Web Targets)
- **Feature**: Transparent `OPTIONS` request management and header exposure mappings.
- **Why**: Emscripten (Web) targets issuing a cross-origin HTTP request to a token server (e.g., from `localhost:8080` to `auth.example.com`) will be blocked by the browser if CORS isn't explicitly supported and properly modeled by the HTTP layer.
- **Exhaustive Requirements**:
  - Support for custom `Access-Control-Request-Headers` modeling natively inside the `HttpRequest` struct abstractions.

## 3. High-Level Authorization Header Management
- **Feature**: First-class macros and generation utilities for Authentication schemes.
- **Why**: Once a token is retrieved, all subsequent API requests need to easily attach the token.
- **Exhaustive Requirements**:
  - Support for `Authorization: Bearer <token>` injection.
  - Support for `Content-Type: application/x-www-form-urlencoded` injection and payload validation for the initial token exchange.
