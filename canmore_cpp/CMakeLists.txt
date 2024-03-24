# Don't compile C++ library library if we're running on the device
if (NOT DEFINED PICO_ON_DEVICE)
    add_library(canmore_cpp STATIC)
    set_target_properties(canmore_cpp PROPERTIES VERSION "${PROJECT_VERSION}")
    set_target_properties(canmore_cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)

    target_sources(canmore_cpp PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/RegMappedClient.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/RegMappedCANClient.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/RegMappedEthernetClient.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/RegMappedServer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/RemoteTTYStream.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/CANSocket.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/MsgAgent.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/MsgClient.cpp
    )

    target_include_directories(canmore PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>" # for headers when building
        "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"  # For headers when installing
    )

    target_link_libraries(canmore_cpp PUBLIC canmore)

    # Export Library
    include(GNUInstallDirs)
    install(TARGETS canmore_cpp
        EXPORT export_${PROJECT_NAME}
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
    # Export include paths
    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
        EXPORT export_${PROJECT_NAME}
        DESTINATION "include/${PROJECT_NAME}"
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
    )
endif()
