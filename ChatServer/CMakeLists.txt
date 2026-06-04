cmake_minimum_required(VERSION 3.16)

project(ChatServer LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql Network)

add_executable(ChatServer
  main.cpp
  dbhelper.cpp
  dbhelper.h
  chatserver.h
  chatserver.cpp
  ../common/netpacket.h
  ../common/netpacket.cpp
)
target_link_libraries(ChatServer PRIVATE Qt6::Core
  Qt6::Sql
  Qt6::Network
)
