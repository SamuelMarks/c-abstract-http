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
FetchContent_Declare(
    c89stringutils
    GIT_REPOSITORY https://github.com/offscale/c89stringutils.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(c89stringutils)

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
        find_package(CURL REQUIRED)
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
        find_package(CURL REQUIRED)
        target_link_libraries(c-abstract-http PUBLIC CURL::libcurl)
    endif()
endif()

enable_testing()
add_subdirectory(tests)
