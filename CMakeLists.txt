cmake_minimum_required(VERSION 3.10)
project(c-abstract-http C)

# Set C standards
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


include(FetchContent)
FetchContent_Declare(
    c89stringutils
    GIT_REPOSITORY https://github.com/offscale/c89stringutils.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(c89stringutils)

option(C_ABSTRACT_HTTP_HEADER_ONLY "Use header-only STB-style library" OFF)
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
    add_library(c-abstract-http STATIC
    src/http_types.c
    src/str.c
    src/fs.c
    src/transport.c
)

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

# Tests
endif()

enable_testing()
add_subdirectory(tests)
