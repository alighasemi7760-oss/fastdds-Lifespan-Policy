cmake_minimum_required(VERSION 3.10)
project(dds_qos_project CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fastdds REQUIRED)

# Standard Binary
add_executable(dds_standard DataSample_Standard.cpp)
target_link_libraries(dds_standard fastdds)

# Adaptive Binary (Includes NetworkMonitor)
add_executable(dds_adaptive DataSample_Adaptive.cpp NetworkMonitor.cpp)
target_link_libraries(dds_adaptive fastdds)
