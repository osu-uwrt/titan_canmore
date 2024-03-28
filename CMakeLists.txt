cmake_minimum_required(VERSION 3.12)

project(titan_canmore VERSION 2.0.0 DESCRIPTION "Library implementing the CANmore Protocol used on OSU UWRT's CAN Bus")

# Only define ament package if we're a top level include
if(NOT NO_AMENT)
    find_package(ament_cmake REQUIRED)
endif()

# Include all the libraries we want
add_subdirectory(canmore/)
add_subdirectory(canmore_cpp/)

if(NOT NO_AMENT)
    ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
    ament_package()
endif()
