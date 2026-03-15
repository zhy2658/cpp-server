cmake_minimum_required(VERSION 3.14)
project(KcpServer LANGUAGES CXX C)

# 强制使用 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 导出编译命令(供 clangd/VSCode 代码提示使用)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 如果是 Windows 系统，定义宏以兼容较新的 Windows API
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

# ==========================================
# 1. 依赖管理 (使用 FetchContent 自动下载源码并编译)
# ==========================================
include(FetchContent)

# Abseil (Protobuf 依赖此基础库)
FetchContent_Declare(
  abseil-cpp
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        20230125.3
  GIT_SHALLOW    TRUE  # 只拉取最新一层代码，加快下载速度
)
FetchContent_MakeAvailable(abseil-cpp)

# Protobuf (用于定义和序列化网络通信协议)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "Build tests") # 关闭测试代码编译
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "Build protoc binaries") # 编译 protoc 工具
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "Link static runtime")
FetchContent_Declare(
  protobuf
  GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
  GIT_TAG        v21.12
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(protobuf)

# KCP (可靠的 UDP 传输协议核心逻辑)
FetchContent_Declare(
  kcp
  GIT_REPOSITORY https://github.com/skywind3000/kcp.git
  GIT_TAG        1.7
  GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(kcp)
if(NOT kcp_POPULATED)
  FetchContent_Populate(kcp)
  # 修复 kcp 旧版 CMakeLists.txt 版本不兼容问题
  file(READ "${kcp_SOURCE_DIR}/CMakeLists.txt" KCP_CMAKELISTS)
  string(REPLACE "CMAKE_MINIMUM_REQUIRED(VERSION 2.6)" "CMAKE_MINIMUM_REQUIRED(VERSION 3.5)" KCP_CMAKELISTS "${KCP_CMAKELISTS}")
  file(WRITE "${kcp_SOURCE_DIR}/CMakeLists.txt" "${KCP_CMAKELISTS}")
  add_subdirectory(${kcp_SOURCE_DIR} ${kcp_BINARY_DIR})
endif()

# ASIO (底层异步网络库，用于处理 UDP Socket)
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-28-0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

# spdlog (高性能日志库 - 暂未启用)
# find_package(spdlog QUIET)
# if(NOT spdlog_FOUND)
#     FetchContent_Declare(
#       spdlog
#       GIT_REPOSITORY https://github.com/gabime/spdlog.git
#       GIT_TAG        v1.12.0
#     )
#     FetchContent_MakeAvailable(spdlog)
# endif()

# yaml-cpp (用于解析 config.yaml 配置文件)
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        yaml-cpp-0.7.0
  GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(yaml-cpp)
if(NOT yaml_cpp_POPULATED)
  FetchContent_Populate(yaml-cpp)
  file(READ "${yaml-cpp_SOURCE_DIR}/CMakeLists.txt" YAML_CPP_CMAKELISTS)
  string(REPLACE "cmake_minimum_required(VERSION 3.1)" "cmake_minimum_required(VERSION 3.5)" YAML_CPP_CMAKELISTS "${YAML_CPP_CMAKELISTS}")
  file(WRITE "${yaml-cpp_SOURCE_DIR}/CMakeLists.txt" "${YAML_CPP_CMAKELISTS}")
  file(READ "${yaml-cpp_SOURCE_DIR}/src/emitterutils.cpp" YAML_CPP_EMITTERUTILS)
  string(REPLACE "#include \"yaml-cpp/null.h\"" "#include \"yaml-cpp/null.h\"\n#include <cstdint>" YAML_CPP_EMITTERUTILS "${YAML_CPP_EMITTERUTILS}")
  file(WRITE "${yaml-cpp_SOURCE_DIR}/src/emitterutils.cpp" "${YAML_CPP_EMITTERUTILS}")
  add_subdirectory(${yaml-cpp_SOURCE_DIR} ${yaml-cpp_BINARY_DIR})
endif()


# ==========================================
# 2. Protobuf 代码自动生成
# ==========================================
file(GLOB PROTO_FILES "proto/*.proto") # 查找 proto 目录下所有 .proto 文件

# 查找 protoc 编译器路径 (优先用刚才编译出来的)
if(TARGET protobuf::protoc)
    set(PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
else()
    find_program(PROTOC_EXECUTABLE protoc)
endif()

if(NOT PROTOC_EXECUTABLE)
    message(FATAL_ERROR "protoc executable not found")
endif()

# 遍历每个 .proto 文件，添加自定义命令将其编译为 .pb.cc 和 .pb.h
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

# ==========================================
# 3. 收集业务源码
# ==========================================
file(GLOB_RECURSE SOURCES "src/*.cpp") # 递归查找 src 下所有 .cpp 文件

# ==========================================
# 4. 创建最终的可执行文件目标
# ==========================================
# 把我们的源码和 protobuf 生成的源码一起打包成 server.exe
add_executable(server ${SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})

# ==========================================
# 5. 配置头文件搜索路径 (Include Directories)
# ==========================================
# 当代码里写 #include 时，编译器去这些目录下找
target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src         # 项目的 src 目录 (包含 core, network, engine, gameplay)
    ${CMAKE_CURRENT_BINARY_DIR}             # 存放生成的 .pb.h 目录
    ${asio_SOURCE_DIR}/asio/include         # asio 库头文件
    ${kcp_SOURCE_DIR}                       # kcp 库头文件
    ${protobuf_SOURCE_DIR}/src              # protobuf 库头文件
)

# ==========================================
# 6. 配置链接库 (Link Libraries)
# ==========================================
# 把第三方库的二进制代码链接到我们的程序中
target_link_libraries(server PRIVATE
    kcp                     # 链接 KCP
    protobuf::libprotobuf   # 链接 Protobuf
    # spdlog::spdlog
    yaml-cpp                # 链接 yaml-cpp
)

# 操作系统特定库链接
if(WIN32)
    target_link_libraries(server PRIVATE ws2_32) # Windows 下必须链接的网络底层库
else()
    target_link_libraries(server PRIVATE pthread) # Linux 下必须链接的多线程库
endif()
