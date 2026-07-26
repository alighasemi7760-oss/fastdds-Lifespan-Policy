cmake_minimum_required(VERSION 3.10)
project(DataSample)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fastdds REQUIRED)

add_executable(DataSample 
    src/DataSample.cpp 
    DataSamplePubSubTypes.cxx 
    DataSampleTypeObjectSupport.cxx
)

target_link_libraries(DataSample PRIVATE fastdds)
