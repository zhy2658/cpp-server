cmake_minimum_required(VERSION 3.14)
project(KcpServer LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Dependencies
include(FetchContent)

# Abseil (Required by Protobuf)
FetchContent_Declare(
  abseil-cpp
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        20230125.3
)
FetchContent_MakeAvailable(abseil-cpp)

# Protobuf
set(protobuf_BUILD_TESTS OFF CACHE BOOL "Build tests")
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "Build protoc binaries")
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "Link static runtime")
FetchContent_Declare(
  protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
  GIT_TAG        v21.12
)
FetchContent_MakeAvailable(protobuf)

# KCP
FetchContent_Declare(
  kcp
  GIT_REPOSITORY https://github.com/skywind3000/kcp.git
  GIT_TAG        1.7
)
FetchContent_GetProperties(kcp)
if(NOT kcp_POPULATED)
  FetchContent_Populate(kcp)
  # Patch CMakeLists.txt to upgrade minimum version
  file(READ "${kcp_SOURCE_DIR}/CMakeLists.txt" KCP_CMAKELISTS)
  string(REPLACE "CMAKE_MINIMUM_REQUIRED(VERSION 2.6)" "CMAKE_MINIMUM_REQUIRED(VERSION 3.5)" KCP_CMAKELISTS "${KCP_CMAKELISTS}")
  file(WRITE "${kcp_SOURCE_DIR}/CMakeLists.txt" "${KCP_CMAKELISTS}")
  add_subdirectory(${kcp_SOURCE_DIR} ${kcp_BINARY_DIR})
endif()

# ASIO (Standalone)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-28-0
)
FetchContent_MakeAvailable(asio)

# spdlog
# find_package(spdlog QUIET)
# if(NOT spdlog_FOUND)
#     FetchContent_Declare(
#       spdlog
#       GIT_REPOSITORY https://github.com/gabime/spdlog.git
#       GIT_TAG        v1.12.0
#     )
#     FetchContent_MakeAvailable(spdlog)
# endif()

# yaml-cpp
# find_package(yaml-cpp QUIET)
# if(NOT yaml-cpp_FOUND)
#     FetchContent_Declare(
#       yaml-cpp
#       GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
#       GIT_TAG        yaml-cpp-0.7.0
#     )
#     FetchContent_MakeAvailable(yaml-cpp)
# endif()

# 2. Proto Generation
file(GLOB PROTO_FILES "proto/*.proto")

# Check if we have protoc executable from system or build
if(TARGET protobuf::protoc)
    set(PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
else()
    # Try finding system protoc
    find_program(PROTOC_EXECUTABLE protoc)
endif()

if(NOT PROTOC_EXECUTABLE)
    message(FATAL_ERROR "protoc executable not found")
endif()

# Manual generation since protobuf_generate_cpp might not be available or compatible
foreach(PROTO ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO} NAME_WE)
    set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
    set(PROTO_HDR "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
    
    add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR}
        COMMAND ${PROTOC_EXECUTABLE}
        ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR} -I${CMAKE_CURRENT_SOURCE_DIR}/proto ${PROTO}
        DEPENDS ${PROTO}
        COMMENT "Generating ${PROTO_NAME}.pb.cc/h"
    )
    
    list(APPEND PROTO_SRCS ${PROTO_SRC})
    list(APPEND PROTO_HDRS ${PROTO_HDR})
endforeach()

# 3. Source Files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 4. Executable
add_executable(server ${SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})

# 5. Include Directories
target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${asio_SOURCE_DIR}/asio/include
    ${kcp_SOURCE_DIR}
    ${protobuf_SOURCE_DIR}/src
)

# 6. Link Libraries
target_link_libraries(server PRIVATE
    kcp
    protobuf::libprotobuf
    # spdlog::spdlog
    # yaml-cpp
)

if(WIN32)
    target_link_libraries(server PRIVATE ws2_32)
else()
    target_link_libraries(server PRIVATE pthread)
endif()
