cmake_minimum_required(VERSION 3.22)
project(TabulaRasa)

set(CMAKE_CXX_STANDARD 14)

include_directories(TabulaRasa)

add_executable(TabulaRasa
        TabulaRasa/constants.h
        TabulaRasa/dominoes.cpp
        TabulaRasa/dominos.h
        TabulaRasa/json.h
        TabulaRasa/main.cpp
        TabulaRasa/messageQueue.h)
