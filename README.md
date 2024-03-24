# Titan CANmore Library

This library contains all of the code for the OSU UWRT's CANmore CAN Bus Protocol, used in Titan Firmware. This protocol
defines the allocation and formatting of CAN frames as part of the vehicle's CAN bus.

This is a CMake library which can be submoduled into any project that will communicate over CAN Bus. Additionally, it
defines a colcon package so this library can be added to any ROS workspace.

## Defined Libraries

* `canmore`: Header definitions and low level C libraries which implement the CANmore specification
* `canmore_cpp`: C++ wrapper libraries and Linux drivers designed to make CANmore application development easier for
  Linux based hosts
