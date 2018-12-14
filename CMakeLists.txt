cmake_minimum_required(VERSION 3.12)
project(ComputerNetworkProject)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)

include_directories(PlayJamming)
include_directories(transbin)
set(CMAKE_CXX_FLAGS  "-lrt -lm -lasound -pthread")

add_executable(transbin
        transbin/crc-8.c
        transbin/crc-8.h
        transbin/crc.c
        transbin/crc.h
        transbin/main.cpp
        transbin/pastream.cpp
        transbin/pastream.h
        transbin/transmit.cpp
        )

add_executable(PlayJamming
        PlayJamming/PlayJamming.cpp)

add_executable(client
        client/client_v2.c)

add_executable(server
        server/server_v2.c)

target_link_libraries(transbin portaudio)
target_link_libraries(transbin sndfile)
target_link_libraries(PlayJamming portaudio)
target_link_libraries(PlayJamming sndfile)