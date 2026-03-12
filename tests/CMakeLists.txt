cmake_minimum_required(VERSION 3.10)

# Fetch greatest.h via FetchContent
include(FetchContent)
FetchContent_Declare(
  greatest
  URL "https://raw.githubusercontent.com/SamuelMarks/greatest/refs/heads/cmake-and-msvc/greatest.h"
  DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(greatest)

include_directories(${greatest_SOURCE_DIR})
include_directories(../include)
include_directories(../src)

# Need to build cdd_test_helpers/mock_server.c as part of the test executable
set(TEST_SOURCES main.c)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    list(APPEND TEST_SOURCES cdd_test_helpers/mock_server.c)
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

if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET c-abstract-http-test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:c-abstract-http>
            $<TARGET_FILE_DIR:c-abstract-http-test>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:c89stringutils>
            $<TARGET_FILE_DIR:c-abstract-http-test>
    )
endif()
