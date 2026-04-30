cmake_minimum_required(VERSION 3.16)
project(chatserver VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Concurrent Sql)

add_executable(${PROJECT_NAME}
    main.cpp
    chatserver.cpp
    clienthandler.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
    Qt6::Sql
)
