cmake_minimum_required(VERSION 3.15...3.31)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(_CAH_PRE_TOP_LEVEL ON)
else()
    set(_CAH_PRE_TOP_LEVEL OFF)
endif()

if(_CAH_PRE_TOP_LEVEL
   AND CMAKE_HOST_APPLE
   AND (NOT CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
   AND NOT CMAKE_TOOLCHAIN_FILE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10" CACHE STRING "Minimum OS X deployment version")
    endif()
    if(NOT CMAKE_OSX_SYSROOT)
        execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE _CAH_MACOS_SDK
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_CAH_MACOS_SDK AND EXISTS "${_CAH_MACOS_SDK}")
            set(CMAKE_OSX_SYSROOT "${_CAH_MACOS_SDK}" CACHE PATH "macOS SDK sysroot")
        endif()
    endif()
endif()

project(c-abstract-http LANGUAGES C)

if(CMAKE_SYSTEM_NAME STREQUAL "DOS" AND WATCOM)
    string(REPLACE "-d2" "-os -d0" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REPLACE "debug all" "" CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
    if(NOT CMAKE_EXE_LINKER_FLAGS MATCHES "opt eliminate")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} opt eliminate")
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "DOS" AND WATCOM
   AND NOT CMAKE_C_COMPILER MATCHES "wcl386"
   AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "I386")
    set(_CAH_DOS_16BIT ON)
else()
    set(_CAH_DOS_16BIT OFF)
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(C_ABSTRACT_HTTP_IS_TOP_LEVEL ON)
else()
    set(C_ABSTRACT_HTTP_IS_TOP_LEVEL OFF)
endif()

include(GNUInstallDirs)
include(FetchContent)

# ------------------------------------------------------------------------------
# Build Options & Configuration
# ------------------------------------------------------------------------------
option(C_ABSTRACT_HTTP_BUILD_TESTING "Build tests" ${C_ABSTRACT_HTTP_IS_TOP_LEVEL})
option(BUILD_TESTING_${PROJECT_NAME} "Build tests" ${C_ABSTRACT_HTTP_BUILD_TESTING})
if(NOT C_ABSTRACT_HTTP_BUILD_TESTING)
    set(BUILD_TESTING_${PROJECT_NAME} OFF)
endif()
if(BUILD_TESTING_${PROJECT_NAME})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.valgrind.supp" AND NOT MEMORYCHECK_SUPPRESSIONS_FILE)
        set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/.valgrind.supp" CACHE FILEPATH "Valgrind suppressions file")
    endif()
    include(CTest)
endif()
set(_CAH_SAVED_BUILD_TESTING "${BUILD_TESTING}")
option(BUILD_SHARED_LIBS "Build Shared Library" OFF)
option(C_ABSTRACT_HTTP_HEADER_ONLY "Use header-only STB-style library" OFF)

set(C_ABSTRACT_HTTP_CRT_LINKAGE "Shared" CACHE STRING "MSVC CRT Linkage: Static or Shared")
set(C_ABSTRACT_HTTP_CHARSET "Unicode" CACHE STRING "Charset to use: Unicode or ANSI")
set(C_ABSTRACT_HTTP_THREADING "Multi-threaded" CACHE STRING "Threading: Multi-threaded or Single-threaded")
set(C_ABSTRACT_HTTP_MSVC_RTC "" CACHE STRING "MSVC Runtime Checks: RTC1, RTCs, RTCu, or empty")
option(C_ABSTRACT_HTTP_LTO "Enable Link-Time Optimization (LTO)" OFF)

option(C_ABSTRACT_HTTP_USE_PICOQUIC "Use picoquic instead of libcurl" OFF)
option(C_ABSTRACT_HTTP_USE_MSH3 "Use msh3 instead of libcurl" OFF)
option(C_ABSTRACT_HTTP_USE_LIBSOUP3 "Use libsoup3 instead of libcurl" OFF)
option(C_ABSTRACT_HTTP_USE_LIBUV "Use libuv instead of libcurl" OFF)
option(C_ABSTRACT_HTTP_USE_LIBEVENT "Use libevent instead of libcurl" OFF)
option(C_ABSTRACT_HTTP_USE_LIBFETCH "Use libfetch instead of libcurl" OFF)
option(C_ABSTRACT_HTTP_USE_RAW_SOCKETS "Use raw BSD/Winsock sockets HTTP/1.1 fallback" OFF)
option(C_ABSTRACT_HTTP_DOS_WATT32 "Enable Watt-32 BSD socket TCP/IP stack on MS-DOS / FreeDOS" OFF)
set(WATT32_INCLUDE_DIR "" CACHE PATH "Optional Watt-32 include directory")
set(WATT32_LIBRARY "" CACHE FILEPATH "Optional Watt-32 library path")
option(C_ABSTRACT_HTTP_USE_MBEDTLS "Use mbedtls for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_OPENSSL "Use OpenSSL for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_LIBRESSL "Use LibreSSL for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_BORINGSSL "Use BoringSSL for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_WOLFSSL "Use wolfSSL for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_S2N "Use s2n-tls for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_BEARSSL "Use BearSSL for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_SCHANNEL "Use Schannel for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_GNUTLS "Use GnuTLS for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_BOTAN "Use Botan for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_COMMONCRYPTO "Use CommonCrypto for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_WINCRYPT "Use wincrypt for cryptography" OFF)
option(C_ABSTRACT_HTTP_USE_WININET "Use WinINet HTTP backend (legacy Windows / MSVC 2005)" OFF)
option(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION "Enable c-multiplatform integration" ON)
option(C_ABSTRACT_HTTP_USE_ASAN "Enable AddressSanitizer" OFF)
option(C_ABSTRACT_HTTP_USE_TSAN "Enable ThreadSanitizer" OFF)
option(C_ABSTRACT_HTTP_ENABLE_COVERAGE "Enable code coverage (gcov)" OFF)
option(C_ABSTRACT_HTTP_USE_XQUIC "Use Alibaba xquic HTTP/3 Backend" OFF)
option(C_ABSTRACT_HTTP_USE_LSQUIC "Use LiteSpeed QUIC (lsquic) HTTP/3 Backend" OFF)
option(C_ABSTRACT_HTTP_USE_NGHTTP3 "Use nghttp3 + ngtcp2 HTTP/3 Backend" OFF)
option(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS "Enable WebSocket support" ON)
option(C_ABSTRACT_HTTP_ENABLE_SSE "Enable Server-Sent Events support" ON)
option(C_ABSTRACT_HTTP_APPLE_USE_NSURLSESSION "Default Apple transport to modern NSURLSession C-bridge" OFF)
option(C_ABSTRACT_HTTP_ENABLE_FUZZING "Enable fuzzing targets" OFF)
option(TRY_SYS_LIB "Fallback to system packages if vcpkg is not available" ON)

set(C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE 1048576 CACHE STRING "Max WS frame size")
set(C_ABSTRACT_HTTP_SSE_MAX_LINE_SIZE 32768 CACHE STRING "Max SSE line size")

# Normalize c-ci / CDD_* overrides before validation
if(DEFINED CDD_CRT_LINKAGE AND NOT "${CDD_CRT_LINKAGE}" STREQUAL "")
    set(C_ABSTRACT_HTTP_CRT_LINKAGE "${CDD_CRT_LINKAGE}" CACHE STRING "" FORCE)
endif()
if(DEFINED CDD_CHARSET AND NOT "${CDD_CHARSET}" STREQUAL "")
    set(C_ABSTRACT_HTTP_CHARSET "${CDD_CHARSET}" CACHE STRING "" FORCE)
endif()
if(DEFINED CDD_THREADING AND NOT "${CDD_THREADING}" STREQUAL "")
    if(CDD_THREADING STREQUAL "OFF" OR CDD_THREADING STREQUAL "Single-threaded" OR NOT CDD_THREADING)
        set(C_ABSTRACT_HTTP_THREADING "Single-threaded" CACHE STRING "" FORCE)
    else()
        set(C_ABSTRACT_HTTP_THREADING "Multi-threaded" CACHE STRING "" FORCE)
    endif()
endif()
if(DEFINED CDD_MSVC_RTC AND NOT "${CDD_MSVC_RTC}" STREQUAL "")
    set(C_ABSTRACT_HTTP_MSVC_RTC "${CDD_MSVC_RTC}" CACHE STRING "" FORCE)
endif()

if(C_ABSTRACT_HTTP_THREADING STREQUAL "Single-threaded" OR C_ABSTRACT_HTTP_THREADING STREQUAL "OFF" OR NOT C_ABSTRACT_HTTP_THREADING)
    set(_CAH_IS_SINGLE_THREADED ON)
else()
    set(_CAH_IS_SINGLE_THREADED OFF)
endif()

set(_CAH_MSVC_RUNTIME_LIBRARY "")
if(MSVC)
    string(TOUPPER "${C_ABSTRACT_HTTP_CRT_LINKAGE}" _CAH_CRT_UPPER)
    if(_CAH_CRT_UPPER STREQUAL "STATIC")
        set(_CAH_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    elseif(_CAH_CRT_UPPER STREQUAL "SHARED" OR _CAH_CRT_UPPER STREQUAL "DYNAMIC")
        set(_CAH_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    elseif(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY AND NOT "${CMAKE_MSVC_RUNTIME_LIBRARY}" STREQUAL "")
        set(_CAH_MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    else()
        set(_CAH_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()

if(MSVC AND BUILD_SHARED_LIBS AND (_CAH_MSVC_RUNTIME_LIBRARY MATCHES "^MultiThreaded(Debug)?$" OR C_ABSTRACT_HTTP_CRT_LINKAGE STREQUAL "Static"))
    message(WARNING "Testing a Shared Library (DLL) with Static CRT (/MT, /MTd) is unsupported due to cross-boundary heap corruption. Disabling tests.")
    set(BUILD_TESTING_${PROJECT_NAME} OFF)
endif()

if((C_ABSTRACT_HTTP_ENABLE_COVERAGE OR "${CMAKE_C_FLAGS}" MATCHES "--coverage")
   AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(C_ABSTRACT_HTTP_COVERAGE_ACTIVE ON)
else()
    set(C_ABSTRACT_HTTP_COVERAGE_ACTIVE OFF)
endif()

if(C_ABSTRACT_HTTP_IS_TOP_LEVEL)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON CACHE BOOL "Skip system libs")
    set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON)
    set(CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY ON)
    set(CMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY ON)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin" "${CMAKE_BINARY_DIR}/lib")
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ws_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ws_config.h"
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sse_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/sse_config.h"
)

# ------------------------------------------------------------------------------
# Dependency Resolution Helpers
# ------------------------------------------------------------------------------
if(DEFINED FETCHCONTENT_SOURCE_DIR_C_FS AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_CFS)
    set(FETCHCONTENT_SOURCE_DIR_CFS "${FETCHCONTENT_SOURCE_DIR_C_FS}" CACHE PATH "" FORCE)
endif()
if(EXISTS "/c89stringutils/CMakeLists.txt" AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_C89STRINGUTILS)
    set(FETCHCONTENT_SOURCE_DIR_C89STRINGUTILS "/c89stringutils" CACHE PATH "" FORCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../c89stringutils/CMakeLists.txt" AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_C89STRINGUTILS)
    set(FETCHCONTENT_SOURCE_DIR_C89STRINGUTILS "${CMAKE_CURRENT_SOURCE_DIR}/../c89stringutils" CACHE PATH "" FORCE)
endif()
if(EXISTS "/c-fs/CMakeLists.txt" AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_CFS)
    set(FETCHCONTENT_SOURCE_DIR_CFS "/c-fs" CACHE PATH "" FORCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../c-fs/CMakeLists.txt" AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_CFS)
    set(FETCHCONTENT_SOURCE_DIR_CFS "${CMAKE_CURRENT_SOURCE_DIR}/../c-fs" CACHE PATH "" FORCE)
endif()
if(EXISTS "/greatest/greatest.h" AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_GREATEST)
    set(FETCHCONTENT_SOURCE_DIR_GREATEST "/greatest" CACHE PATH "" FORCE)
endif()

if(CYGWIN)
    foreach(_dep c89stringutils cfs)
        string(TOUPPER "${_dep}" _dep_upper)
        set(_var_name "FETCHCONTENT_SOURCE_DIR_${_dep_upper}")
        if(DEFINED CACHE{${_var_name}})
            file(TO_CMAKE_PATH "$CACHE{${_var_name}}" _val)
            if(_val MATCHES "^([a-zA-Z]):/(.*)")
                string(TOLOWER "${CMAKE_MATCH_1}" _drive_letter)
                set(${_var_name} "/cygdrive/${_drive_letter}/${CMAKE_MATCH_2}" CACHE PATH "" FORCE)
            endif()
        endif()
    endforeach()
endif()

function(c_abstract_http_suppress_target_warnings tgt)
    if(NOT TARGET ${tgt})
        return()
    endif()
    get_target_property(_tgt_type ${tgt} TYPE)
    if(_tgt_type STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()
    get_target_property(_tgt_imported ${tgt} IMPORTED)
    if(_tgt_imported)
        return()
    endif()
    if(MSVC)
        get_target_property(_pkg_opts ${tgt} COMPILE_OPTIONS)
        if(_pkg_opts)
            string(REGEX REPLACE "[-/]W([0-4]|X)" "" _pkg_opts "${_pkg_opts}")
            string(REGEX REPLACE "\\$<[^>]+:>" "" _pkg_opts "${_pkg_opts}")
            set_target_properties(${tgt} PROPERTIES COMPILE_OPTIONS "${_pkg_opts}")
        endif()
        target_compile_options(${tgt} PRIVATE /W0)
    elseif(WATCOM)
        target_compile_options(${tgt} PRIVATE -w0)
    else()
        target_compile_options(${tgt} PRIVATE -w)
    endif()
endfunction()

function(c_abstract_http_mark_system_includes tgt)
    if(TARGET ${tgt})
        get_target_property(_sys_inc ${tgt} INTERFACE_INCLUDE_DIRECTORIES)
        if(_sys_inc)
            set_target_properties(${tgt} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_sys_inc}")
        endif()
    endif()
endfunction()

function(c_abstract_http_resolve_dependency PkgName FetchRepo FetchTag)
    set(_vcpkg_applicable ON)
    if(MINGW OR CYGWIN OR (MSVC AND MSVC_VERSION LESS 1500))
        set(_vcpkg_applicable OFF)
    endif()

    set(_resolved OFF)
    string(TOUPPER "${PkgName}" _pkg_upper)
    if(TARGET ${PkgName} OR ${PkgName}_FOUND OR ${_pkg_upper}_FOUND)
        message(STATUS "${PkgName} already provided by parent")
        set(_resolved ON)
    endif()

    if(NOT _resolved AND _vcpkg_applicable AND DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg[.]cmake")
        find_package(${PkgName} CONFIG QUIET)
        if(${PkgName}_FOUND)
            message(STATUS "Found ${PkgName} via vcpkg")
            set(_resolved ON)
        endif()
    endif()

    if(NOT _resolved
       AND (TRY_SYS_LIB OR "${PkgName}" STREQUAL "CURL")
       AND NOT "${PkgName}" STREQUAL "c89stringutils"
       AND NOT "${PkgName}" STREQUAL "cfs")
        find_package(${PkgName} QUIET)
        if(${PkgName}_FOUND OR ${_pkg_upper}_FOUND)
            message(STATUS "Found ${PkgName} via system packages")
            set(_resolved ON)
        endif()
    endif()

    if(NOT _resolved AND NOT "${FetchRepo}" STREQUAL "")
        message(STATUS "Fetching ${PkgName} via FetchContent")
        set(BUILD_TESTING OFF)
        set(CMAKE_C_EXTENSIONS ON)
        if(MSVC AND _CAH_MSVC_RUNTIME_LIBRARY)
            set(CMAKE_MSVC_RUNTIME_LIBRARY "${_CAH_MSVC_RUNTIME_LIBRARY}")
        endif()
        unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        unset(CMAKE_LIBRARY_OUTPUT_DIRECTORY)
        unset(CMAKE_ARCHIVE_OUTPUT_DIRECTORY)

        FetchContent_Declare(${PkgName}
            GIT_REPOSITORY ${FetchRepo}
            GIT_TAG ${FetchTag}
        )
        FetchContent_MakeAvailable(${PkgName})
        if(DEFINED CACHE{BUILD_TESTING} AND NOT "${_CAH_SAVED_BUILD_TESTING}" STREQUAL "")
            set(BUILD_TESTING "${_CAH_SAVED_BUILD_TESTING}" CACHE BOOL "Build the testing tree." FORCE)
        endif()

        c_abstract_http_suppress_target_warnings(${PkgName})
        if("${PkgName}" STREQUAL "CURL" AND TARGET libcurl)
            set_target_properties(libcurl PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)
            c_abstract_http_suppress_target_warnings(libcurl)
            if(NOT MSVC AND NOT WATCOM)
                target_compile_definitions(libcurl PRIVATE "inline=__inline__")
            endif()
        endif()
        if(TARGET ${PkgName}_compiler_flags)
            set_target_properties(${PkgName}_compiler_flags PROPERTIES INTERFACE_COMPILE_OPTIONS "")
        endif()
        set(_resolved ON)
    endif()

    if(NOT _resolved)
        message(FATAL_ERROR "Failed to resolve dependency '${PkgName}'. Install it via system packages / vcpkg or check network access.")
    endif()
endfunction()

function(c_abstract_http_resolve_core_dependencies)
    set(BUILD_TESTING OFF)
    if(MSVC AND _CAH_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "${_CAH_MSVC_RUNTIME_LIBRARY}")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "DOS")
        set(CFS_MULTITHREADED OFF CACHE BOOL "" FORCE)
        set(C89STRINGUTILS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
        c_abstract_http_resolve_dependency(c89stringutils https://github.com/offscale/c89stringutils.git master)
        if(TARGET c89stringutils AND _CAH_DOS_16BIT)
            get_target_property(_c89_type c89stringutils TYPE)
            if(NOT _c89_type STREQUAL "INTERFACE_LIBRARY")
                target_compile_options(c89stringutils PRIVATE -ml)
            endif()
        endif()
        c_abstract_http_mark_system_includes(c89stringutils)
    else()
        if(_CAH_IS_SINGLE_THREADED)
            set(CFS_MULTITHREADED OFF CACHE BOOL "" FORCE)
            set(CFS_THREADING "Single-threaded" CACHE STRING "" FORCE)
        endif()
        set(BUILD_SHARED_LIBS OFF)
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
        set(C89STRINGUTILS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(CFS_BUILD_TESTS OFF CACHE BOOL "" FORCE)

        c_abstract_http_resolve_dependency(c89stringutils https://github.com/offscale/c89stringutils.git master)
        c_abstract_http_resolve_dependency(cfs https://github.com/SamuelMarks/c-fs.git master)

        if(TARGET cfs AND APPLE)
            target_compile_definitions(cfs PUBLIC _DARWIN_C_SOURCE)
        endif()
        c_abstract_http_mark_system_includes(c89stringutils)
        c_abstract_http_mark_system_includes(cfs)
    endif()
endfunction()

c_abstract_http_resolve_core_dependencies()

function(c_abstract_http_resolve_curl)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF)
    if(C_ABSTRACT_HTTP_USE_OPENSSL OR C_ABSTRACT_HTTP_USE_BORINGSSL OR C_ABSTRACT_HTTP_USE_LIBRESSL)
        set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    elseif(C_ABSTRACT_HTTP_USE_MBEDTLS)
        set(CURL_USE_MBEDTLS ON CACHE BOOL "" FORCE)
    elseif(C_ABSTRACT_HTTP_USE_BEARSSL)
        set(CURL_USE_BEARSSL ON CACHE BOOL "" FORCE)
    elseif(C_ABSTRACT_HTTP_USE_WOLFSSL)
        set(CURL_USE_WOLFSSL ON CACHE BOOL "" FORCE)
    elseif(C_ABSTRACT_HTTP_USE_GNUTLS)
        set(CURL_USE_GNUTLS ON CACHE BOOL "" FORCE)
    elseif(C_ABSTRACT_HTTP_USE_SCHANNEL)
        set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    elseif(C_ABSTRACT_HTTP_USE_COMMONCRYPTO)
        set(CURL_USE_SECTRANSP ON CACHE BOOL "" FORCE)
    else()
        find_package(OpenSSL QUIET)
        if(NOT OPENSSL_FOUND)
            set(CURL_ENABLE_SSL OFF CACHE BOOL "" FORCE)
            set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
            set(OPENSSL_CRYPTO_LIBRARY "" CACHE INTERNAL "" FORCE)
            set(OPENSSL_SSL_LIBRARY "" CACHE INTERNAL "" FORCE)
            set(OPENSSL_INCLUDE_DIR "" CACHE INTERNAL "" FORCE)
        endif()
    endif()
    c_abstract_http_resolve_dependency(CURL https://github.com/curl/curl.git curl-8_0_1)
endfunction()

function(c_abstract_http_apply_crypto tgt pub_scope priv_scope)
    if(C_ABSTRACT_HTTP_USE_MBEDTLS)
        c_abstract_http_resolve_dependency(mbedtls https://github.com/Mbed-TLS/mbedtls.git v3.4.0)
        target_link_libraries(${tgt} ${priv_scope} mbedtls mbedcrypto mbedx509)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_MBEDTLS)
    elseif(C_ABSTRACT_HTTP_USE_OPENSSL)
        c_abstract_http_resolve_dependency(OpenSSL "" "")
        target_link_libraries(${tgt} ${priv_scope} OpenSSL::SSL OpenSSL::Crypto)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_OPENSSL)
    elseif(C_ABSTRACT_HTTP_USE_LIBRESSL)
        c_abstract_http_resolve_dependency(LibreSSL https://github.com/libressl/portable.git v3.8.0)
        target_link_libraries(${tgt} ${priv_scope} LibreSSL::TLS LibreSSL::Crypto)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_LIBRESSL)
    elseif(C_ABSTRACT_HTTP_USE_BORINGSSL)
        c_abstract_http_resolve_dependency(BoringSSL https://github.com/google/boringssl.git master)
        target_link_libraries(${tgt} ${priv_scope} crypto ssl)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_BORINGSSL)
    elseif(C_ABSTRACT_HTTP_USE_WOLFSSL)
        set(WOLFSSL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(WOLFSSL_CRYPT_TESTS OFF CACHE BOOL "" FORCE)
        c_abstract_http_resolve_dependency(wolfssl https://github.com/wolfSSL/wolfssl.git v5.6.6-stable)
        target_link_libraries(${tgt} ${priv_scope} wolfssl)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_WOLFSSL)
    elseif(C_ABSTRACT_HTTP_USE_S2N)
        c_abstract_http_resolve_dependency(s2n https://github.com/aws/s2n-tls.git main)
        target_link_libraries(${tgt} ${priv_scope} s2n::s2n)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_S2N)
    elseif(C_ABSTRACT_HTTP_USE_BEARSSL)
        find_package(bearssl QUIET)
        if(TARGET bearssl::bearssl)
            target_link_libraries(${tgt} ${priv_scope} bearssl::bearssl)
        elseif(TARGET bearssl)
            target_link_libraries(${tgt} ${priv_scope} bearssl)
        else()
            find_library(_CAH_BEARSSL_LIB NAMES bearssl REQUIRED)
            find_path(_CAH_BEARSSL_INC NAMES bearssl.h REQUIRED)
            target_include_directories(${tgt} ${priv_scope} "${_CAH_BEARSSL_INC}")
            target_link_libraries(${tgt} ${priv_scope} "${_CAH_BEARSSL_LIB}")
        endif()
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_BEARSSL)
    elseif(C_ABSTRACT_HTTP_USE_SCHANNEL AND WIN32)
        target_link_libraries(${tgt} ${priv_scope} secur32 crypt32 bcrypt)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_SCHANNEL)
    elseif(C_ABSTRACT_HTTP_USE_GNUTLS)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GNUTLS REQUIRED IMPORTED_TARGET gnutls)
        target_link_libraries(${tgt} ${priv_scope} PkgConfig::GNUTLS)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_GNUTLS)
    elseif(C_ABSTRACT_HTTP_USE_BOTAN)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(BOTAN REQUIRED IMPORTED_TARGET botan-3)
        target_link_libraries(${tgt} ${priv_scope} PkgConfig::BOTAN)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_BOTAN)
    elseif(C_ABSTRACT_HTTP_USE_COMMONCRYPTO AND APPLE)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_COMMONCRYPTO)
    elseif(C_ABSTRACT_HTTP_USE_WINCRYPT AND WIN32)
        target_link_libraries(${tgt} ${priv_scope} crypt32 bcrypt)
        target_compile_definitions(${tgt} ${pub_scope} C_ABSTRACT_HTTP_USE_WINCRYPT)
    endif()
endfunction()

# ------------------------------------------------------------------------------
# Primary Library Target (Unified INTERFACE / STATIC / SHARED)
# ------------------------------------------------------------------------------
if(C_ABSTRACT_HTTP_HEADER_ONLY)
    add_library(c-abstract-http INTERFACE)
    set(_CAH_PUB_SCOPE INTERFACE)
    set(_CAH_PRIV_SCOPE INTERFACE)
else()
    add_library(c-abstract-http
        src/http_types.c
        $<$<BOOL:${C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION}>:src/cmp_integration.c>
        src/event_loop.c
        src/thread_pool.c
        src/c_abstract_http_tls.c
        src/coroutine.c
        src/actor.c
        src/process.c
        src/transport.c
        src/crypto_utils.c
        src/http_raw.c
    )
    set(_CAH_PUB_SCOPE PUBLIC)
    set(_CAH_PRIV_SCOPE PRIVATE)

    set_target_properties(c-abstract-http PROPERTIES
        C_STANDARD 90
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
    if(MSVC AND _CAH_MSVC_RUNTIME_LIBRARY)
        set_target_properties(c-abstract-http PROPERTIES
            MSVC_RUNTIME_LIBRARY "${_CAH_MSVC_RUNTIME_LIBRARY}"
        )
    endif()

    if(BUILD_SHARED_LIBS)
        target_compile_definitions(c-abstract-http PUBLIC C_ABSTRACT_HTTP_BUILD_SHARED=1)
        set_target_properties(c-abstract-http PROPERTIES DEFINE_SYMBOL c_abstract_http_EXPORTS)
        if(CMAKE_HOST_WIN32)
            set_target_properties(c-abstract-http PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
        endif()
        if(MINGW OR CYGWIN)
            target_link_options(c-abstract-http PRIVATE "-Wl,--export-all-symbols")
        endif()
    endif()
endif()

add_library(c-abstract-http::c-abstract-http ALIAS c-abstract-http)

target_include_directories(c-abstract-http ${_CAH_PUB_SCOPE}
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_include_directories(c-abstract-http ${_CAH_PRIV_SCOPE}
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)

# Target-scoped threading, charset, sanitizers, LTO, and compiler warnings
if(_CAH_IS_SINGLE_THREADED)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_SINGLE_THREADED=1)
elseif(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    find_package(Threads REQUIRED)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} Threads::Threads)
endif()

if(NOT C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION=1)
endif()

if(WIN32 OR CYGWIN)
    string(TOUPPER "${C_ABSTRACT_HTTP_CHARSET}" _CAH_CHARSET_UPPER)
    if(_CAH_CHARSET_UPPER STREQUAL "UNICODE")
        target_compile_definitions(c-abstract-http ${_CAH_PRIV_SCOPE} UNICODE _UNICODE)
    elseif(_CAH_CHARSET_UPPER STREQUAL "ANSI" OR _CAH_CHARSET_UPPER STREQUAL "MBCS")
        target_compile_definitions(c-abstract-http ${_CAH_PRIV_SCOPE} _MBCS)
    endif()
endif()

if(_CAH_DOS_16BIT)
    target_compile_options(c-abstract-http ${_CAH_PUB_SCOPE} -ml)
    target_link_options(c-abstract-http ${_CAH_PUB_SCOPE} -ml)
endif()

if(C_ABSTRACT_HTTP_USE_ASAN AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(c-abstract-http ${_CAH_PUB_SCOPE} -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(c-abstract-http ${_CAH_PUB_SCOPE} -fsanitize=address)
endif()

if(C_ABSTRACT_HTTP_USE_TSAN AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(c-abstract-http ${_CAH_PUB_SCOPE} -fsanitize=thread)
    target_link_options(c-abstract-http ${_CAH_PUB_SCOPE} -fsanitize=thread)
endif()

if(MSVC AND C_ABSTRACT_HTTP_MSVC_RTC AND NOT C_ABSTRACT_HTTP_MSVC_RTC STREQUAL "OFF")
    string(REGEX REPLACE "^/" "" _CAH_RTC_CLEAN "${C_ABSTRACT_HTTP_MSVC_RTC}")
    foreach(_cfg_flag CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL)
        if(DEFINED ${_cfg_flag})
            string(REGEX REPLACE "[/-]O[12x]" "" ${_cfg_flag} "${${_cfg_flag}}")
            string(REGEX REPLACE "[/-]RTC[1suc]+" "" ${_cfg_flag} "${${_cfg_flag}}")
        endif()
    endforeach()
    target_compile_options(c-abstract-http ${_CAH_PUB_SCOPE} "/Od" "/${_CAH_RTC_CLEAN}")
elseif(C_ABSTRACT_HTTP_LTO
       AND NOT (APPLE AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
       AND NOT C_ABSTRACT_HTTP_HEADER_ONLY)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _cah_lto_supported)
    if(_cah_lto_supported)
        set_target_properties(c-abstract-http PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
        if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT APPLE AND NOT MSVC AND NOT EMSCRIPTEN)
            find_program(_CAH_LLD_LINKER NAMES ld.lld lld)
            if(_CAH_LLD_LINKER)
                target_link_options(c-abstract-http ${_CAH_PUB_SCOPE} "-fuse-ld=lld")
            endif()
        endif()
    endif()
endif()

if(NOT C_ABSTRACT_HTTP_HEADER_ONLY)
    if(MSVC)
        target_compile_options(c-abstract-http PRIVATE "/W4" "/WX")
    elseif(WATCOM)
        target_compile_options(c-abstract-http PRIVATE "-wx" "-we")
    else()
        include(CheckCCompilerFlag)
        target_compile_options(c-abstract-http PRIVATE
            "-Wall" "-Wextra" "-Werror" "-pedantic"
            "-Wconversion" "-Wsign-conversion" "-Wshadow" "-Wcast-align"
            "-Wcast-qual" "-Wswitch-default" "-Wbad-function-cast"
            "-Wstrict-prototypes" "-Wmissing-prototypes" "-Wmissing-declarations"
            "-Wpointer-arith" "-Wwrite-strings" "-Wold-style-definition"
            "-Wdeclaration-after-statement" "-Wnested-externs" "-Wvla"
            "-Wformat=2" "-Wformat-security"
            "-Wundef" "-Wdouble-promotion" "-Winit-self"
        )
        foreach(_cah_flag
            "-Wnull-dereference"
            "-Wshorten-64-to-32"
            "-Wdocumentation"
            "-Wdocumentation-unknown-command"
            "-Wimplicit-int-enum-cast"
            "-Wassign-enum"
            "-Wcomma"
            "-Wcovered-switch-default"
            "-Wunreachable-code"
            "-Wlogical-op"
            "-Wduplicated-cond"
            "-Wduplicated-branches"
            "-Wjump-misses-init"
        )
            string(MAKE_C_IDENTIFIER "CAH_HAS_${_cah_flag}" _cah_flag_var)
            set(_cah_saved_req "${CMAKE_REQUIRED_FLAGS}")
            set(CMAKE_REQUIRED_FLAGS "-Werror")
            check_c_compiler_flag("${_cah_flag}" ${_cah_flag_var})
            set(CMAKE_REQUIRED_FLAGS "${_cah_saved_req}")
            if(${_cah_flag_var})
                target_compile_options(c-abstract-http PRIVATE "${_cah_flag}")
            endif()
        endforeach()
        if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_definitions(c-abstract-http PRIVATE "inline=__inline__" "_Bool=int" "__forceinline=__inline__")
        endif()
        if(APPLE)
            target_compile_definitions(c-abstract-http PRIVATE _DARWIN_C_SOURCE)
        elseif(NOT WIN32)
            target_compile_definitions(c-abstract-http PRIVATE _DEFAULT_SOURCE _BSD_SOURCE _POSIX_C_SOURCE=200809L)
        endif()
    endif()
endif()

# Core utility dependencies
if(CMAKE_SYSTEM_NAME STREQUAL "DOS")
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} c89stringutils)
else()
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} c89stringutils cfs)
endif()

function(c_abstract_http_add_compiled_source src_file)
    if(NOT C_ABSTRACT_HTTP_HEADER_ONLY)
        target_sources(c-abstract-http PRIVATE "${src_file}")
    endif()
endfunction()

if(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
    c_abstract_http_add_compiled_source(src/http_ws.c)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
endif()

if(C_ABSTRACT_HTTP_ENABLE_SSE)
    c_abstract_http_add_compiled_source(src/http_sse.c)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_ENABLE_SSE)
endif()

# ------------------------------------------------------------------------------
# Unified Platform & Transport Backend Selection
# ------------------------------------------------------------------------------
if(EMSCRIPTEN)
    c_abstract_http_add_compiled_source(src/http_wasm.c)
    target_link_options(c-abstract-http ${_CAH_PUB_SCOPE} "SHELL:-s FETCH=1")
elseif(C_ABSTRACT_HTTP_USE_LIBUV)
    c_abstract_http_add_compiled_source(src/http_libuv.c)
    c_abstract_http_resolve_dependency(libuv https://github.com/libuv/libuv.git v1.48.0)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} uv_a)
    if(WIN32)
        target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} psapi userenv iphlpapi ws2_32)
    endif()
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_LIBUV)
elseif(C_ABSTRACT_HTTP_USE_LIBEVENT)
    c_abstract_http_add_compiled_source(src/http_libevent.c)
    if(C_ABSTRACT_HTTP_USE_OPENSSL OR C_ABSTRACT_HTTP_USE_LIBRESSL OR C_ABSTRACT_HTTP_USE_BORINGSSL)
        set(EVENT__DISABLE_OPENSSL OFF CACHE BOOL "" FORCE)
    else()
        set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "" FORCE)
    endif()
    set(EVENT__DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "" FORCE)
    set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)
    c_abstract_http_resolve_dependency(libevent https://github.com/libevent/libevent.git release-2.1.12-stable)
    if(MSVC AND MSVC_VERSION LESS 1600)
        if(TARGET event)
            get_target_property(_ev_imported event IMPORTED)
            if(NOT _ev_imported)
                target_compile_definitions(event PRIVATE UINT32_MAX=0xffffffff)
            endif()
        endif()
        target_compile_definitions(c-abstract-http ${_CAH_PRIV_SCOPE} UINT32_MAX=0xffffffff)
    endif()
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} event)
    if(C_ABSTRACT_HTTP_USE_OPENSSL OR C_ABSTRACT_HTTP_USE_LIBRESSL OR C_ABSTRACT_HTTP_USE_BORINGSSL)
        if(TARGET event_openssl)
            target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} event_openssl)
        elseif(TARGET libevent::openssl)
            target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} libevent::openssl)
        endif()
    endif()
    if(WIN32)
        target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} ws2_32 advapi32 iphlpapi)
    endif()
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_LIBEVENT)
elseif(C_ABSTRACT_HTTP_USE_PICOQUIC)
    c_abstract_http_add_compiled_source(src/http_picoquic.c)
    c_abstract_http_resolve_dependency(mbedtls https://github.com/Mbed-TLS/mbedtls.git v3.4.0)
    set(WITH_MBEDTLS ON CACHE BOOL "" FORCE)
    set(PICOQUIC_FETCH_PTLS ON CACHE BOOL "" FORCE)
    c_abstract_http_resolve_dependency(picoquic https://github.com/private-octopus/picoquic.git master)
    FetchContent_GetProperties(picoquic)
    if(picoquic_SOURCE_DIR)
        target_include_directories(c-abstract-http ${_CAH_PRIV_SCOPE}
            "$<BUILD_INTERFACE:${picoquic_SOURCE_DIR}/picoquic>"
            "$<BUILD_INTERFACE:${picoquic_SOURCE_DIR}/picoquic-mbedtls>"
        )
    endif()
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} picoquic mbedtls mbedcrypto mbedx509)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_PICOQUIC C_ABSTRACT_HTTP_HAVE_REAL_PICOQUIC)
elseif(C_ABSTRACT_HTTP_USE_MSH3)
    c_abstract_http_add_compiled_source(src/http_msh3.c)
    c_abstract_http_resolve_dependency(msh3 https://github.com/nibanks/msh3.git main)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} msh3)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_MSH3 C_ABSTRACT_HTTP_HAVE_REAL_MSH3)
elseif(C_ABSTRACT_HTTP_USE_XQUIC)
    c_abstract_http_add_compiled_source(src/http_xquic.c)
    c_abstract_http_resolve_dependency(xquic https://github.com/alibaba/xquic.git master)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} xquic)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_XQUIC C_ABSTRACT_HTTP_HAVE_REAL_XQUIC)
elseif(C_ABSTRACT_HTTP_USE_LSQUIC)
    c_abstract_http_add_compiled_source(src/http_lsquic.c)
    c_abstract_http_resolve_dependency(lsquic https://github.com/litespeedtech/lsquic.git master)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} lsquic)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_LSQUIC C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC)
elseif(C_ABSTRACT_HTTP_USE_NGHTTP3)
    c_abstract_http_add_compiled_source(src/http_nghttp3.c)
    c_abstract_http_resolve_dependency(ngtcp2 https://github.com/ngtcp2/ngtcp2.git main)
    c_abstract_http_resolve_dependency(nghttp3 https://github.com/ngtcp2/nghttp3.git main)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} nghttp3 ngtcp2)
    foreach(_ngc_lib ngtcp2_crypto_quictls ngtcp2_crypto_ossl ngtcp2_crypto_gnutls ngtcp2_crypto_picotls)
        if(TARGET ${_ngc_lib})
            target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} ${_ngc_lib})
            break()
        else()
            find_library(_CAH_NGC_${_ngc_lib} NAMES ${_ngc_lib})
            if(_CAH_NGC_${_ngc_lib})
                target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} "${_CAH_NGC_${_ngc_lib}}")
                break()
            endif()
        endif()
    endforeach()
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_NGHTTP3 C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
elseif(C_ABSTRACT_HTTP_USE_LIBSOUP3)
    c_abstract_http_add_compiled_source(src/http_libsoup3.c)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSOUP3 REQUIRED IMPORTED_TARGET libsoup-3.0)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} PkgConfig::LIBSOUP3)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_LIBSOUP3 C_ABSTRACT_HTTP_HAVE_REAL_LIBSOUP3)
elseif(C_ABSTRACT_HTTP_USE_LIBFETCH)
    c_abstract_http_add_compiled_source(src/http_fetch.c)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} fetch)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_LIBFETCH C_ABSTRACT_HTTP_HAVE_REAL_FETCH)
elseif(C_ABSTRACT_HTTP_USE_RAW_SOCKETS)
    if(WIN32)
        target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} ws2_32)
    endif()
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_RAW_SOCKETS)
elseif(WIN32 AND NOT MINGW_TEST_CURL)
    if((MSVC AND MSVC_VERSION LESS 1600) OR C_ABSTRACT_HTTP_USE_WININET)
        c_abstract_http_add_compiled_source(src/http_wininet.c)
        target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} wininet ws2_32 crypt32 advapi32 shlwapi)
        target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_WININET)
    else()
        c_abstract_http_add_compiled_source(src/http_winhttp.c)
        target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} winhttp wininet ws2_32 crypt32 advapi32 shlwapi bcrypt)
        target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_WINHTTP)
    endif()
elseif(APPLE AND CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
    c_abstract_http_add_compiled_source(src/http_apple.c)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_APPLE)
    if(C_ABSTRACT_HTTP_APPLE_USE_NSURLSESSION)
        target_compile_definitions(c-abstract-http ${_CAH_PRIV_SCOPE} C_ABSTRACT_HTTP_APPLE_USE_NSURLSESSION=1)
    endif()
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(CFNETWORK_FRAMEWORK CFNetwork REQUIRED)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} "${FOUNDATION_FRAMEWORK}" "${CFNETWORK_FRAMEWORK}" objc)
    if(C_ABSTRACT_HTTP_COVERAGE_ACTIVE AND NOT C_ABSTRACT_HTTP_HEADER_ONLY)
        find_package(CURL QUIET)
        if(CURL_FOUND)
            target_sources(c-abstract-http PRIVATE src/http_curl.c)
            target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} CURL::libcurl)
            target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_HAVE_CURL)
        endif()
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "DOS")
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_USE_RAW_SOCKETS)
    if(C_ABSTRACT_HTTP_DOS_WATT32)
        target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_DOS_WATT32=1)
        if(WATT32_INCLUDE_DIR)
            target_include_directories(c-abstract-http ${_CAH_PRIV_SCOPE} "${WATT32_INCLUDE_DIR}")
        endif()
        if(WATT32_LIBRARY)
            target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} "${WATT32_LIBRARY}")
        else()
            target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} wattcpwf)
        endif()
    endif()
else()
    c_abstract_http_add_compiled_source(src/http_curl.c)
    c_abstract_http_resolve_curl()
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} CURL::libcurl)
    target_compile_definitions(c-abstract-http ${_CAH_PUB_SCOPE} C_ABSTRACT_HTTP_HAVE_CURL)
endif()

c_abstract_http_apply_crypto(c-abstract-http ${_CAH_PUB_SCOPE} ${_CAH_PRIV_SCOPE})

if(WIN32)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} ws2_32 secur32 crypt32)
elseif(APPLE)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(c-abstract-http ${_CAH_PRIV_SCOPE} "${SECURITY_FRAMEWORK}" "${COREFOUNDATION_FRAMEWORK}")
endif()

# ------------------------------------------------------------------------------
# Installation Rules
# ------------------------------------------------------------------------------
install(TARGETS c-abstract-http
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/c_abstract_http"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# ------------------------------------------------------------------------------
# Testing & OOM-Instrumented Library Target
# ------------------------------------------------------------------------------
if(BUILD_TESTING_${PROJECT_NAME})
    if(BUILD_TESTING)
        set(C_ABSTRACT_HTTP_MOCK_DEFS
            C_ABSTRACT_HTTP_TEST_OOM=1
            _ACRTIMP= _CRTIMP= WINSOCK_API_LINKAGE=
            malloc=c_abstract_http_mock_malloc
            calloc=c_abstract_http_mock_calloc
            realloc=c_abstract_http_mock_realloc
            free=c_abstract_http_mock_free
            c_abstract_http_strdup=c_abstract_http_mock_strdup
            fwrite=c_abstract_http_mock_fwrite
            fclose=c_abstract_http_mock_fclose
            socket=c_abstract_http_mock_socket
            bind=c_abstract_http_mock_bind
            listen=c_abstract_http_mock_listen
            accept=c_abstract_http_mock_accept
            recv=c_abstract_http_mock_recv
            getsockname=c_abstract_http_mock_getsockname
            pthread_key_create=c_abstract_http_mock_pthread_key_create
            pthread_mutex_init=c_abstract_http_mock_pthread_mutex_init
            pthread_cond_init=c_abstract_http_mock_pthread_cond_init
            pthread_create=c_abstract_http_mock_pthread_create
            pthread_join=c_abstract_http_mock_pthread_join
            pipe=c_abstract_http_mock_pipe
            fork=c_abstract_http_mock_fork
            waitpid=c_abstract_http_mock_waitpid
            select=c_abstract_http_mock_select
            c_abstract_http_get_time_ms=c_abstract_http_mock_get_time_ms
            pthread_setspecific=c_abstract_http_mock_pthread_setspecific
            pthread_getspecific=c_abstract_http_mock_pthread_getspecific
        )
        if(NOT WIN32)
            list(APPEND C_ABSTRACT_HTTP_MOCK_DEFS execv=c_abstract_http_mock_execv)
        endif()

        if(NOT C_ABSTRACT_HTTP_HEADER_ONLY AND NOT (WIN32 AND BUILD_SHARED_LIBS) AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
            get_target_property(_inst_sources c-abstract-http SOURCES)
            get_target_property(_inst_inc c-abstract-http INCLUDE_DIRECTORIES)
            get_target_property(_inst_pub_inc c-abstract-http INTERFACE_INCLUDE_DIRECTORIES)
            get_target_property(_inst_defs c-abstract-http COMPILE_DEFINITIONS)
            get_target_property(_inst_pub_defs c-abstract-http INTERFACE_COMPILE_DEFINITIONS)
            get_target_property(_inst_opts c-abstract-http COMPILE_OPTIONS)
            get_target_property(_inst_link_opts c-abstract-http LINK_OPTIONS)
            get_target_property(_inst_links c-abstract-http LINK_LIBRARIES)

            set(_inst_all_sources
                ${_inst_sources}
                src/http_picoquic.c
                src/http_msh3.c
                src/http_xquic.c
                src/http_lsquic.c
                src/http_nghttp3.c
                src/http_fetch.c
                src/http_libsoup3.c
                src/http_libevent.c
                src/http_libuv.c
                src/http_winhttp.c
                src/http_wininet.c
                tests/mock_alloc.c
                tests/abstract_http_test_helpers/mock_server.c
            )
            if(NOT EMSCRIPTEN)
                list(APPEND _inst_all_sources src/http_wasm.c)
            endif()
            list(REMOVE_DUPLICATES _inst_all_sources)

            add_library(c-abstract-http-instrumented STATIC ${_inst_all_sources})
            set_target_properties(c-abstract-http-instrumented PROPERTIES
                C_STANDARD 90
                C_STANDARD_REQUIRED ON
                C_EXTENSIONS OFF
            )
            if(MSVC AND _CAH_MSVC_RUNTIME_LIBRARY)
                set_target_properties(c-abstract-http-instrumented PROPERTIES
                    MSVC_RUNTIME_LIBRARY "${_CAH_MSVC_RUNTIME_LIBRARY}"
                )
            endif()

            if(_inst_pub_inc)
                target_include_directories(c-abstract-http-instrumented PUBLIC ${_inst_pub_inc})
            endif()
            if(_inst_inc)
                target_include_directories(c-abstract-http-instrumented PRIVATE ${_inst_inc})
            endif()
            target_include_directories(c-abstract-http-instrumented PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")

            if(_inst_opts)
                target_compile_options(c-abstract-http-instrumented PRIVATE ${_inst_opts})
            endif()
            if(_inst_link_opts)
                list(FILTER _inst_link_opts EXCLUDE REGEX "export-all-symbols")
                if(_inst_link_opts)
                    target_link_options(c-abstract-http-instrumented PUBLIC ${_inst_link_opts})
                endif()
            endif()
            if(_inst_pub_defs)
                list(REMOVE_ITEM _inst_pub_defs "C_ABSTRACT_HTTP_BUILD_SHARED=1")
                if(_inst_pub_defs)
                    target_compile_definitions(c-abstract-http-instrumented PUBLIC ${_inst_pub_defs})
                endif()
            endif()
            if(_inst_defs)
                list(REMOVE_ITEM _inst_defs "C_ABSTRACT_HTTP_BUILD_SHARED=1")
                if(_inst_defs)
                    target_compile_definitions(c-abstract-http-instrumented PRIVATE ${_inst_defs})
                endif()
            endif()
            target_compile_definitions(c-abstract-http-instrumented PRIVATE ${C_ABSTRACT_HTTP_MOCK_DEFS})

            if(_inst_links)
                target_link_libraries(c-abstract-http-instrumented PUBLIC ${_inst_links})
            endif()
            if(C_ABSTRACT_HTTP_COVERAGE_ACTIVE)
                target_compile_options(c-abstract-http-instrumented PRIVATE "--coverage")
                target_link_options(c-abstract-http-instrumented PUBLIC "--coverage")
            endif()
        endif()

        add_subdirectory(tests)
    endif()
endif()
