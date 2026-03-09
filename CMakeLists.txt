cmake_minimum_required(VERSION 3.10)
project(c-abstract-http C)

# Set C standards
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

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
option(BUILD_SHARED_LIBS "Build Shared Library" OFF)
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
                string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
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

include(FetchContent)

macro(c_abstract_http_resolve_dependency PkgName FetchRepo FetchTag)
    set(_VCPKG_APPLICABLE ON)
    if(MINGW OR CYGWIN OR (MSVC AND MSVC_VERSION LESS 1500))
        set(_VCPKG_APPLICABLE OFF)
    endif()

    set(${PkgName}_RESOLVED OFF)

    # a) try vcpkg
    if(_VCPKG_APPLICABLE AND DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake")
        find_package(${PkgName} CONFIG QUIET)
        if(${PkgName}_FOUND)
            message(STATUS "Found ${PkgName} via vcpkg")
            set(${PkgName}_RESOLVED ON)
        endif()
    endif()

    # b) try system if TRY_SYS_LIB is set
    if(NOT ${PkgName}_RESOLVED AND TRY_SYS_LIB)
        find_package(${PkgName} QUIET)
        if(${PkgName}_FOUND)
            message(STATUS "Found ${PkgName} via system packages")
            set(${PkgName}_RESOLVED ON)
        endif()
    endif()

    # c) try fetchcontent
    if(NOT ${PkgName}_RESOLVED)
        message(STATUS "Fetching ${PkgName} via FetchContent")
        FetchContent_Declare(${PkgName}
            GIT_REPOSITORY ${FetchRepo}
            GIT_TAG ${FetchTag}
        )
        FetchContent_MakeAvailable(${PkgName})
        # If fetchcontent fails, CMake usually aborts or the target is not created
        # We assume success if it didn't fatal error during MakeAvailable,
        # but we can do a sanity check to ensure the target we need is there.
        set(${PkgName}_RESOLVED ON)
    endif()

    if(NOT ${PkgName}_RESOLVED)
        message(FATAL_ERROR "Failed to resolve dependency ${PkgName}")
    endif()
endmacro()

c_abstract_http_resolve_dependency(c89stringutils https://github.com/offscale/c89stringutils.git master)

if(C_ABSTRACT_HTTP_HEADER_ONLY)
    add_library(c-abstract-http INTERFACE)
    target_include_directories(c-abstract-http INTERFACE include)
    if(WIN32)
        target_link_libraries(c-abstract-http INTERFACE winhttp wininet ws2_32 crypt32 advapi32 shlwapi bcrypt c89stringutils)
    elseif(APPLE)
        find_library(FOUNDATION Foundation)
        find_library(CFNetwork CFNetwork)
        target_link_libraries(c-abstract-http INTERFACE ${FOUNDATION} ${CFNetwork} c89stringutils)
    else()
        c_abstract_http_resolve_dependency(CURL https://github.com/curl/curl.git curl-8_0_1)
        target_link_libraries(c-abstract-http INTERFACE CURL::libcurl c89stringutils)
    endif()
else()
    if(BUILD_SHARED_LIBS)
        add_library(c-abstract-http SHARED
            src/http_types.c
            src/str.c
            src/fs.c
            src/transport.c
        )
    else()
        add_library(c-abstract-http STATIC
            src/http_types.c
            src/str.c
            src/fs.c
            src/transport.c
        )
    endif()

    target_include_directories(c-abstract-http PUBLIC include)
    target_link_libraries(c-abstract-http PUBLIC c89stringutils)

    # Platform specific HTTP backends
    if(WIN32)
        target_sources(c-abstract-http PRIVATE
            src/http_winhttp.c
            src/http_wininet.c
        )
        target_link_libraries(c-abstract-http PUBLIC winhttp wininet ws2_32 crypt32 advapi32 shlwapi bcrypt)
    elseif(APPLE)
        target_sources(c-abstract-http PRIVATE
            src/http_apple.c
        )
        find_library(FOUNDATION Foundation)
        find_library(CFNetwork CFNetwork)
        target_link_libraries(c-abstract-http PUBLIC ${FOUNDATION} ${CFNetwork})
    else()
        target_sources(c-abstract-http PRIVATE
            src/http_curl.c
        )
        c_abstract_http_resolve_dependency(CURL https://github.com/curl/curl.git curl-8_0_1)
        target_link_libraries(c-abstract-http PUBLIC CURL::libcurl)
    endif()
endif()

enable_testing()
add_subdirectory(tests)
