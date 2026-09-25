cmake_minimum_required(VERSION 3.20)
project(TGTExchange LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(exchange_core order_book.cpp)
target_include_directories(exchange_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(exchange_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(core_tests core_tests.cpp)
target_link_libraries(core_tests PRIVATE exchange_core)
target_compile_options(core_tests PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME core_tests COMMAND core_tests)

option(BUILD_SERVER "Build the Drogon HTTP server" ON)

if(BUILD_SERVER)
    find_package(Drogon CONFIG REQUIRED)

    add_executable(exchange_server server.cpp)
    target_link_libraries(
        exchange_server
        PRIVATE exchange_core Drogon::Drogon
    )
    target_compile_options(
        exchange_server PRIVATE -Wall -Wextra -Wpedantic
    )

endif()