cmake_minimum_required(VERSION 3.12)

project(titan_canmore VERSION 2.0.0 DESCRIPTION "Library implementing the CANmore Protocol used on OSU UWRT's CAN Bus")

# Only define ament package if we're a top level include
if(PROJECT_IS_TOP_LEVEL)
    find_package(ament_cmake REQUIRED)
    set(CMAKE_CXX_STANDARD 17)
endif()

# Include all the libraries we want
add_subdirectory(canmore/)
add_subdirectory(canmore_cpp/)

if(PROJECT_IS_TOP_LEVEL)
    ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
    ament_package()
endif()
