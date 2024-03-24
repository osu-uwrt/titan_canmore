add_library(canmore STATIC)
set_target_properties(canmore PROPERTIES VERSION "${PROJECT_VERSION}")
set_target_properties(canmore PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(canmore PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/crc32.c
    ${CMAKE_CURRENT_LIST_DIR}/src/msg_encoding.c
    ${CMAKE_CURRENT_LIST_DIR}/src/reg_mapped_server.c
    ${CMAKE_CURRENT_LIST_DIR}/src/reg_mapped_client.c
)

target_include_directories(canmore PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>" # for headers when building
    "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"  # For headers when installing
)

# Export Library
include(GNUInstallDirs)
install(TARGETS canmore
    EXPORT export_${PROJECT_NAME}
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
# Export include paths
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
    EXPORT export_${PROJECT_NAME}
    DESTINATION "include/${PROJECT_NAME}"
    FILES_MATCHING PATTERN "*.h"
)
