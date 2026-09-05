cmake_minimum_required(VERSION 3.22)
project(TimeMaster VERSION 5.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(BUILD_TESTING "Build tests" ON)

if(BUILD_TESTING)
    enable_testing()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Svg Network Test)

add_subdirectory(src)

add_subdirectory(tests)
