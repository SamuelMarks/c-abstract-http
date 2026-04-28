cmake_minimum_required(VERSION 3.10)

# Fetch greatest.h via FetchContent
include(FetchContent)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../greatest/greatest.h")
    message(STATUS "Using local greatest for tests")
    set(greatest_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../greatest")
else()
    FetchContent_Declare(
      greatest
      URL "https://raw.githubusercontent.com/SamuelMarks/greatest/refs/heads/master/greatest.h"
      DOWNLOAD_NO_EXTRACT TRUE
    )
    FetchContent_MakeAvailable(greatest)
endif()

include_directories(${greatest_SOURCE_DIR})
include_directories(../include)
include_directories(../src)

# Need to build cdd_test_helpers/mock_server.c as part of the test executable
set(TEST_SOURCES main.c mock_alloc.c)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    list(APPEND TEST_SOURCES cdd_test_helpers/mock_server.c mock_alloc.c)
endif()

add_executable(c-abstract-http-test ${TEST_SOURCES})
target_link_libraries(c-abstract-http-test PRIVATE c-abstract-http)

if(C_ABSTRACT_HTTP_HEADER_ONLY)
    target_compile_definitions(c-abstract-http-test PRIVATE C_ABSTRACT_HTTP_HEADER_ONLY)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    find_package(Threads REQUIRED)
    target_link_libraries(c-abstract-http-test PRIVATE Threads::Threads)
endif()

if(NOT C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION)
    target_compile_definitions(c-abstract-http-test PRIVATE C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
endif()

if(WIN32)
    target_link_libraries(c-abstract-http-test PRIVATE ws2_32)
endif()

add_test(NAME c-abstract-http-tests COMMAND c-abstract-http-test)

if(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
    add_executable(c-abstract-http-test-ws test_ws.c mock_alloc.c)
    target_link_libraries(c-abstract-http-test-ws PRIVATE c-abstract-http)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        target_link_libraries(c-abstract-http-test-ws PRIVATE Threads::Threads)
    endif()
    if(WIN32)
        target_link_libraries(c-abstract-http-test-ws PRIVATE ws2_32)
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(c-abstract-http-test-ws PRIVATE -std=c89 -pedantic -Werror)
    elseif(MSVC)
        target_compile_options(c-abstract-http-test-ws PRIVATE /WX)
    endif()
    add_test(NAME c-abstract-http-test-ws COMMAND c-abstract-http-test-ws)
endif()

if(C_ABSTRACT_HTTP_ENABLE_SSE)
    add_executable(c-abstract-http-test-sse test_sse.c mock_alloc.c)
    target_link_libraries(c-abstract-http-test-sse PRIVATE c-abstract-http)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        target_link_libraries(c-abstract-http-test-sse PRIVATE Threads::Threads)
    endif()
    if(WIN32)
        target_link_libraries(c-abstract-http-test-sse PRIVATE ws2_32)
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(c-abstract-http-test-sse PRIVATE -std=c89 -pedantic -Werror)
    elseif(MSVC)
        target_compile_options(c-abstract-http-test-sse PRIVATE /WX)
    endif()
    add_test(NAME c-abstract-http-test-sse COMMAND c-abstract-http-test-sse)
endif()

if(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
    add_executable(c-abstract-http-test-crypto test_crypto.c mock_alloc.c)
    target_link_libraries(c-abstract-http-test-crypto PRIVATE c-abstract-http)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        target_link_libraries(c-abstract-http-test-crypto PRIVATE Threads::Threads)
    endif()
    if(WIN32)
        target_link_libraries(c-abstract-http-test-crypto PRIVATE ws2_32)
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(c-abstract-http-test-crypto PRIVATE -std=c89 -pedantic -Werror)
    elseif(MSVC)
        target_compile_options(c-abstract-http-test-crypto PRIVATE /WX)
    endif()
    add_test(NAME c-abstract-http-test-crypto COMMAND c-abstract-http-test-crypto)
endif()

if(C_ABSTRACT_HTTP_ENABLE_FUZZING)
    if(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
        add_executable(fuzz_ws fuzz_ws.c)
        target_link_libraries(fuzz_ws PRIVATE c-abstract-http)
        target_compile_options(fuzz_ws PRIVATE -fsanitize=fuzzer)
        target_link_options(fuzz_ws PRIVATE -fsanitize=fuzzer)
        add_test(NAME c-abstract-http-fuzz-ws COMMAND fuzz_ws -runs=1000)
    endif()
    if(C_ABSTRACT_HTTP_ENABLE_SSE)
        add_executable(fuzz_sse fuzz_sse.c)
        target_link_libraries(fuzz_sse PRIVATE c-abstract-http)
        target_compile_options(fuzz_sse PRIVATE -fsanitize=fuzzer)
        target_link_options(fuzz_sse PRIVATE -fsanitize=fuzzer)
        add_test(NAME c-abstract-http-fuzz-sse COMMAND fuzz_sse -runs=1000)
    endif()
endif()

if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET c-abstract-http-test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:c-abstract-http>
            $<TARGET_FILE_DIR:c-abstract-http-test>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:c89stringutils>
            $<TARGET_FILE_DIR:c-abstract-http-test>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:cfs>
            $<TARGET_FILE_DIR:c-abstract-http-test>
    )
endif()
