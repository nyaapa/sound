cmake_minimum_required(VERSION 3.3)
project(sound)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -g -O0 -D_REENTRANT -Wall -Werror")
set(CMAKE_EXE_LINKER_FLAGS  "-L/home/nyaapa/ccrtp/src/.libs -lccrtp -lucommon -lcommoncpp -lpthread -lrt -lportaudio -lsndfile")
include_directories(include /home/nyaapa/ccrtp/src/)

add_executable(sender src/sender.cpp)
add_executable(receiver src/receiver.cpp include/stream_player.h)
add_executable(play main.cpp)