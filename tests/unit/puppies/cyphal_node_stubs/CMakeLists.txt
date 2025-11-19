# Library to be used by cyphal_node untitests

add_library(cyphal_node_test_stubs INTERFACE)

set(CYPHAL_NODE_TEST_STUBS_SOURCES
    ${CMAKE_SOURCE_DIR}/src/puppy/honeybee_shared/cyphal_node.cpp
    ${CMAKE_SOURCE_DIR}/src/puppy/honeybee_shared/salted_app_hash_command_server.cpp
    ${CMAKE_SOURCE_DIR}/src/can/cyphal_proto_suber.cpp
    ${CMAKE_SOURCE_DIR}/src/can/cyphal_proto_sender.cpp
    ${CMAKE_SOURCE_DIR}/src/can/cyphal_task.cpp
    ${CMAKE_SOURCE_DIR}/src/can/cyphal_timesync.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/puppy_impl_stubs.cpp
    PARENT_SCOPE
    )

target_include_directories(
  cyphal_node_test_stubs
  INTERFACE include_stub
            include_stub/freertos
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/can
            ${CMAKE_SOURCE_DIR}/src/common
            ${CMAKE_SOURCE_DIR}/src/puppy/honeybee_shared
            ${CMAKE_SOURCE_DIR}/src/puppy/can_shared
  )

target_link_libraries(
  cyphal_node_test_stubs INTERFACE SG14 canard dsdl_uavcan dsdl_prusa3d buddy_utils
  )
