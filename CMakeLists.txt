cmake_minimum_required(VERSION 3.10)
project(c-abstract-http C)

#Set C standards
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(C_ABSTRACT_HTTP_HEADER_ONLY "Use header-only STB-style library" OFF)
option(C_ABSTRACT_HTTP_STATIC_CRT "Use Static CRT (/MT, /MTd)" OFF)
option(C_ABSTRACT_HTTP_SHARED_CRT "Use Shared CRT (/MD, /MDd) (default)" ON)
option(C_ABSTRACT_HTTP_UNICODE "Use UNICODE" OFF)
option(C_ABSTRACT_HTTP_ANSI "Use ANSI (Multi-Byte)" OFF)
option(C_ABSTRACT_HTTP_MULTI_THREADED "Multi-threaded" ON)
option(C_ABSTRACT_HTTP_SINGLE_THREADED "Single-threaded" OFF)
option(C_ABSTRACT_HTTP_LTO "Enable Link Time Optimization" OFF)
option(C_ABSTRACT_HTTP_RTC1 "Enable /RTC1" OFF)
option(C_ABSTRACT_HTTP_RTCs "Enable /RTCs" OFF)
option(C_ABSTRACT_HTTP_RTCu "Enable /RTCu" OFF)
option(C_ABSTRACT_HTTP_USE_LIBSOUP3 "Use libsoup3 instead of libcurl" OFF)
option(C_ABSTRACT_HTTP_USE_LIBUV "Use libuv instead of libcurl" OFF)
option(C_ABSTRACT_HTTP_USE_LIBEVENT "Use libevent instead of libcurl" OFF)
option(C_ABSTRACT_HTTP_USE_LIBFETCH "Use libfetch instead of libcurl" OFF)
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
option(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION "Enable c-multiplatform integration" ON)
option(BUILD_SHARED_LIBS "Build Shared Library" OFF)
option(C_ABSTRACT_HTTP_USE_ASAN "Enable AddressSanitizer" OFF)
option(C_ABSTRACT_HTTP_USE_TSAN "Enable ThreadSanitizer" OFF)
option(TRY_SYS_LIB "Fallback to system packages if vcpkg is not available" ON)

if(C_ABSTRACT_HTTP_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_supported)
    if(lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

if(MSVC)
    if(C_ABSTRACT_HTTP_STATIC_CRT)
        foreach(flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
            if(${flag_var} MATCHES "/MD")
                set(_tmp "${${flag_var}}")
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${_tmp}")
            endif()
        endforeach()
    endif()
    
    if(C_ABSTRACT_HTTP_UNICODE)
        add_definitions(-DUNICODE -D_UNICODE)
    elseif(C_ABSTRACT_HTTP_ANSI)
        add_definitions(-D_MBCS)
    endif()
    
    if(C_ABSTRACT_HTTP_RTC1)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /RTC1")
    endif()
    if(C_ABSTRACT_HTTP_RTCs)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /RTCs")
    endif()
    if(C_ABSTRACT_HTTP_RTCu)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /RTCu")
    endif()
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY ON)
set(CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY ON)
set(CMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY ON)
include(FetchContent)

macro(c_abstract_http_resolve_dependency PkgName FetchRepo FetchTag)
    set(_VCPKG_APPLICABLE ON)
    if(MINGW OR CYGWIN OR (MSVC AND MSVC_VERSION LESS 1500))
        set(_VCPKG_APPLICABLE OFF)
    endif()

    set(${PkgName}_RESOLVED OFF)

#a) try vcpkg
    if(_VCPKG_APPLICABLE AND DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake")
        find_package(${PkgName} CONFIG QUIET)
        if(${PkgName}_FOUND)
            message(STATUS "Found ${PkgName} via vcpkg")
            set(${PkgName}_RESOLVED ON)
        endif()
    endif()

#b) try system if TRY_SYS_LIB is set
    if(NOT ${PkgName}_RESOLVED AND TRY_SYS_LIB AND NOT ${PkgName} STREQUAL "c89stringutils" AND NOT ${PkgName} STREQUAL "cfs")
        find_package(${PkgName} QUIET NO_CMAKE_BUILDS_PATH)
        if(${PkgName}_FOUND)
            message(STATUS "Found ${PkgName} via system packages")
            set(${PkgName}_RESOLVED ON)
        endif()
    endif()

#c) try fetchcontent
    if(NOT ${PkgName}_RESOLVED)
        message(STATUS "Fetching ${PkgName} via FetchContent")
        FetchContent_Declare(${PkgName}
            GIT_REPOSITORY ${FetchRepo}
            GIT_TAG ${FetchTag}
            GIT_CONFIG "core.autocrlf=false"
            UPDATE_COMMAND ""
        )
        FetchContent_MakeAvailable(${PkgName})
#If fetchcontent fails, CMake usually aborts or the target is not created
#We assume success if it didn't fatal error during MakeAvailable,
#but we can do a sanity check to ensure the target we need is there.
        set(${PkgName}_RESOLVED ON)
    endif()

    if(NOT ${PkgName}_RESOLVED)
        message(FATAL_ERROR "Failed to resolve dependency ${PkgName}")
    endif()
endmacro()

c_abstract_http_resolve_dependency(c89stringutils https://github.com/offscale/c89stringutils.git master)

if(CMAKE_SYSTEM_NAME STREQUAL "DOS")
    set(CFS_MULTITHREADED OFF CACHE BOOL "" FORCE)
else()
    c_abstract_http_resolve_dependency(cfs https://github.com/SamuelMarks/c-fs.git master)
endif()

macro(c_abstract_http_apply_crypto tgt scope)
    if(C_ABSTRACT_HTTP_USE_MBEDTLS)
        c_abstract_http_resolve_dependency(mbedtls https://github.com/Mbed-TLS/mbedtls.git v3.4.0)
        target_link_libraries(${tgt} ${scope} mbedtls mbedcrypto mbedx509)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_MBEDTLS)
    elseif(C_ABSTRACT_HTTP_USE_OPENSSL)
        c_abstract_http_resolve_dependency(OpenSSL https://github.com/openssl/openssl.git openssl-3.1.2)
        target_link_libraries(${tgt} ${scope} OpenSSL::SSL OpenSSL::Crypto)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_OPENSSL)
    elseif(C_ABSTRACT_HTTP_USE_LIBRESSL)
        c_abstract_http_resolve_dependency(LibreSSL https://github.com/libressl/portable.git v3.8.0)
        target_link_libraries(${tgt} ${scope} LibreSSL::TLS LibreSSL::Crypto)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_LIBRESSL)
    elseif(C_ABSTRACT_HTTP_USE_BORINGSSL)
        c_abstract_http_resolve_dependency(BoringSSL https://github.com/google/boringssl.git master)
        target_link_libraries(${tgt} ${scope} crypto ssl)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_BORINGSSL)
    elseif(C_ABSTRACT_HTTP_USE_WOLFSSL)
        set(WOLFSSL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(WOLFSSL_CRYPT_TESTS OFF CACHE BOOL "" FORCE)
        c_abstract_http_resolve_dependency(wolfssl https://github.com/wolfSSL/wolfssl.git v5.6.6-stable)
        target_link_libraries(${tgt} ${scope} wolfssl)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_WOLFSSL)
    elseif(C_ABSTRACT_HTTP_USE_S2N)
        set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
        c_abstract_http_resolve_dependency(s2n https://github.com/aws/s2n-tls.git main)
        target_link_libraries(${tgt} ${scope} s2n::s2n)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_S2N)
    elseif(C_ABSTRACT_HTTP_USE_BEARSSL)
        c_abstract_http_resolve_dependency(bearssl https://github.com/bearssl/bearssl.git master)
        target_link_libraries(${tgt} ${scope} bearssl)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_BEARSSL)
    elseif(C_ABSTRACT_HTTP_USE_SCHANNEL AND WIN32)
        target_link_libraries(${tgt} ${scope} secur32 crypt32 bcrypt)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_SCHANNEL)
    elseif(C_ABSTRACT_HTTP_USE_GNUTLS)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GNUTLS REQUIRED gnutls)
        target_include_directories(${tgt} ${scope} ${GNUTLS_INCLUDE_DIRS})
        target_link_libraries(${tgt} ${scope} ${GNUTLS_LIBRARIES})
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_GNUTLS)
    elseif(C_ABSTRACT_HTTP_USE_BOTAN)
        c_abstract_http_resolve_dependency(botan https://github.com/randombit/botan.git master)
        target_link_libraries(${tgt} ${scope} botan-3)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_BOTAN)
    elseif(C_ABSTRACT_HTTP_USE_COMMONCRYPTO AND APPLE)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_COMMONCRYPTO)
    elseif(C_ABSTRACT_HTTP_USE_WINCRYPT AND WIN32)
        target_link_libraries(${tgt} ${scope} crypt32 bcrypt)
        target_compile_definitions(${tgt} ${scope} C_ABSTRACT_HTTP_USE_WINCRYPT)
    endif()
endmacro()

macro(c_abstract_http_configure_curl)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
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
    endif()
endmacro()

if(C_ABSTRACT_HTTP_HEADER_ONLY)
    add_library(c-abstract-http INTERFACE)
    target_include_directories(c-abstract-http INTERFACE include)
    if(CMAKE_SYSTEM_NAME STREQUAL "Android" OR ANDROID)
        target_link_libraries(c-abstract-http INTERFACE c89stringutils cfs)
    elseif(EMSCRIPTEN)
        target_link_options(c-abstract-http INTERFACE "SHELL:-s FETCH=1")
        target_link_libraries(c-abstract-http INTERFACE c89stringutils cfs)
    elseif(C_ABSTRACT_HTTP_USE_LIBUV)
        c_abstract_http_resolve_dependency(libuv https://github.com/libuv/libuv.git v1.48.0)
        target_link_libraries(c-abstract-http INTERFACE uv_a c89stringutils cfs)
        if(WIN32)
            target_link_libraries(c-abstract-http INTERFACE psapi userenv iphlpapi ws2_32)
        endif()
        target_compile_definitions(c-abstract-http INTERFACE C_ABSTRACT_HTTP_USE_LIBUV)
    elseif(C_ABSTRACT_HTTP_USE_LIBEVENT)
        set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_TESTS ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)
        if(MSVC AND MSVC_VERSION LESS 1600)
            add_compile_definitions(UINT32_MAX=0xffffffff)
        endif()
        c_abstract_http_resolve_dependency(libevent https://github.com/libevent/libevent.git release-2.1.12-stable)
        target_link_libraries(c-abstract-http INTERFACE event c89stringutils cfs)
        if(WIN32)
            target_link_libraries(c-abstract-http INTERFACE ws2_32 advapi32 iphlpapi)
        endif()
        target_compile_definitions(c-abstract-http INTERFACE C_ABSTRACT_HTTP_USE_LIBEVENT)
    elseif(WIN32 AND NOT MINGW_TEST_CURL)
        if(MSVC AND MSVC_VERSION LESS 1600)
            target_link_libraries(c-abstract-http INTERFACE wininet ws2_32 crypt32 advapi32 shlwapi c89stringutils cfs)
        else()
            target_link_libraries(c-abstract-http INTERFACE winhttp wininet ws2_32 crypt32 advapi32 shlwapi bcrypt c89stringutils cfs)
        endif()
    elseif(APPLE)
        find_library(FOUNDATION Foundation)
        find_library(CFNetwork CFNetwork)
        target_link_libraries(c-abstract-http INTERFACE ${FOUNDATION} ${CFNetwork} c89stringutils cfs)
    elseif(C_ABSTRACT_HTTP_USE_LIBSOUP3)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBSOUP3 REQUIRED libsoup-3.0)
        target_include_directories(c-abstract-http INTERFACE ${LIBSOUP3_INCLUDE_DIRS})
        target_link_libraries(c-abstract-http INTERFACE ${LIBSOUP3_LIBRARIES} c89stringutils cfs)
        target_compile_definitions(c-abstract-http INTERFACE C_ABSTRACT_HTTP_USE_LIBSOUP3)
    elseif(C_ABSTRACT_HTTP_USE_LIBFETCH)
        target_link_libraries(c-abstract-http INTERFACE fetch c89stringutils cfs)
        target_compile_definitions(c-abstract-http INTERFACE C_ABSTRACT_HTTP_USE_LIBFETCH)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "DOS")
        target_compile_definitions(c-abstract-http INTERFACE C_ABSTRACT_HTTP_USE_RAW_SOCKETS)
    else()
        c_abstract_http_resolve_dependency(CURL https://github.com/curl/curl.git curl-8_0_1)
        target_link_libraries(c-abstract-http INTERFACE CURL::libcurl c89stringutils cfs)
    endif()
    
    c_abstract_http_apply_crypto(c-abstract-http INTERFACE)
else()
    if(BUILD_SHARED_LIBS)
        add_library(c-abstract-http SHARED
            src/http_types.c
            $<$<BOOL:${C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION}>:src/cmp_integration.c>
            
            src/event_loop.c
            src/thread_pool.c
            src/cdd_tls.c
            src/coroutine.c
            src/actor.c
            src/process.c
            src/str.c
            src/transport.c
        )
        set_target_properties(c-abstract-http PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    else()
        add_library(c-abstract-http STATIC
            src/http_types.c
            $<$<BOOL:${C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION}>:src/cmp_integration.c>
            
            src/event_loop.c
            src/thread_pool.c
            src/cdd_tls.c
            src/coroutine.c
            src/actor.c
            src/process.c
            src/str.c
            src/transport.c
        )
    endif()

    target_include_directories(c-abstract-http PUBLIC include)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        target_link_libraries(c-abstract-http PUBLIC c89stringutils cfs)
    else()
        target_link_libraries(c-abstract-http PUBLIC c89stringutils)
    endif()

#Platform specific HTTP backends
    if(CMAKE_SYSTEM_NAME STREQUAL "Android" OR ANDROID)
        target_sources(c-abstract-http PRIVATE
            src/http_android.c
        )
    elseif(EMSCRIPTEN)
        target_sources(c-abstract-http PRIVATE
            src/http_wasm.c
        )
#Fetch API requires - s FETCH = 1
        target_link_options(c-abstract-http PUBLIC "SHELL:-s FETCH=1")
    elseif(C_ABSTRACT_HTTP_USE_LIBUV)
        target_sources(c-abstract-http PRIVATE
            src/http_libuv.c
        )
        c_abstract_http_resolve_dependency(libuv https://github.com/libuv/libuv.git v1.48.0)
        target_link_libraries(c-abstract-http PUBLIC uv_a)
        if(WIN32)
            target_link_libraries(c-abstract-http PUBLIC psapi userenv iphlpapi ws2_32)
        endif()
        target_compile_definitions(c-abstract-http PUBLIC C_ABSTRACT_HTTP_USE_LIBUV)
    elseif(C_ABSTRACT_HTTP_USE_LIBEVENT)
        target_sources(c-abstract-http PRIVATE
            src/http_libevent.c
        )
        set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_TESTS ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)
        if(MSVC AND MSVC_VERSION LESS 1600)
            add_compile_definitions(UINT32_MAX=0xffffffff)
        endif()
        c_abstract_http_resolve_dependency(libevent https://github.com/libevent/libevent.git release-2.1.12-stable)
        target_link_libraries(c-abstract-http PUBLIC event)
        if(WIN32)
            target_link_libraries(c-abstract-http PUBLIC ws2_32 advapi32 iphlpapi)
        endif()
        target_compile_definitions(c-abstract-http PUBLIC C_ABSTRACT_HTTP_USE_LIBEVENT)
    elseif(WIN32 AND NOT MINGW_TEST_CURL)
        target_sources(c-abstract-http PRIVATE
            src/http_winhttp.c
            src/http_wininet.c
        )
        if(MSVC AND MSVC_VERSION LESS 1600)
            target_link_libraries(c-abstract-http PUBLIC wininet ws2_32)
        else()
            target_link_libraries(c-abstract-http PUBLIC winhttp wininet ws2_32 crypt32 advapi32 shlwapi bcrypt)
        endif()
    elseif(APPLE)
        target_sources(c-abstract-http PRIVATE
            src/http_apple.c
        )
        find_library(FOUNDATION Foundation)
        find_library(CFNetwork CFNetwork)
        target_link_libraries(c-abstract-http PUBLIC ${FOUNDATION} ${CFNetwork})
    elseif(C_ABSTRACT_HTTP_USE_LIBSOUP3)
        target_sources(c-abstract-http PRIVATE
            src/http_libsoup3.c
        )
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBSOUP3 REQUIRED libsoup-3.0)
        target_include_directories(c-abstract-http PUBLIC ${LIBSOUP3_INCLUDE_DIRS})
        target_link_libraries(c-abstract-http PUBLIC ${LIBSOUP3_LIBRARIES})
        target_compile_definitions(c-abstract-http PUBLIC C_ABSTRACT_HTTP_USE_LIBSOUP3)
    elseif(C_ABSTRACT_HTTP_USE_LIBFETCH)
        target_sources(c-abstract-http PRIVATE
            src/http_fetch.c
        )
        target_link_libraries(c-abstract-http PUBLIC fetch)
        target_compile_definitions(c-abstract-http PUBLIC C_ABSTRACT_HTTP_USE_LIBFETCH)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "DOS")
        target_sources(c-abstract-http PRIVATE
            src/http_raw.c
        )
        target_compile_definitions(c-abstract-http PUBLIC C_ABSTRACT_HTTP_USE_RAW_SOCKETS)
    else()
        target_sources(c-abstract-http PRIVATE
            src/http_curl.c
        )
        c_abstract_http_configure_curl()
        c_abstract_http_resolve_dependency(CURL https://github.com/curl/curl.git curl-8_0_1)
        target_link_libraries(c-abstract-http PUBLIC CURL::libcurl)
    endif()
    
    c_abstract_http_apply_crypto(c-abstract-http PUBLIC)
endif()

enable_testing()
add_subdirectory(tests)


if(C_ABSTRACT_HTTP_USE_ASAN)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
  endif()
endif()

if(C_ABSTRACT_HTTP_USE_TSAN)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
  endif()
endif()

