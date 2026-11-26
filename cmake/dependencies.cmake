# dependencies.cmake - Manage project dependencies

# Auto-detect chen-sdk directory by pattern "chen-sdk-*"
file(GLOB CHEN_SDK_DIRS "${PROJECT_SOURCE_DIR}/chen-sdk-*")
if (CHEN_SDK_DIRS)
    list(GET CHEN_SDK_DIRS 0 CHEN_SDK_DIR)
    message(STATUS "Using CHEN_SDK_DIR=${CHEN_SDK_DIR}")
else()
    message(FATAL_ERROR "chen-sdk-* not found under ${PROJECT_SOURCE_DIR}. Please fetch chen-sdk.")
endif()

include_directories(${CHEN_SDK_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/compat)
include_directories(${PROJECT_SOURCE_DIR}/dbproxy/data)

# 三方依赖（由 deps.py sync / thirdparty 下载到 deps/：头文件 + 静态库）
# 仅用于编译 chen 头文件（protobuf/yaml-cpp/tinyxml2/jsoncpp/hiredis-vip），不静态链接进模块，
# 运行时符号由预编译 libchen.so 提供，避免重复符号。
set(THIRDPARTY_DEPS_DIR ${PROJECT_SOURCE_DIR}/deps)
if (EXISTS ${THIRDPARTY_DEPS_DIR}/include)
    include_directories(${THIRDPARTY_DEPS_DIR}/include)
    list(PREPEND CMAKE_PREFIX_PATH ${THIRDPARTY_DEPS_DIR})
    list(PREPEND CMAKE_LIBRARY_PATH ${THIRDPARTY_DEPS_DIR}/lib)
    list(PREPEND CMAKE_INCLUDE_PATH ${THIRDPARTY_DEPS_DIR}/include)
    set(Protobuf_ROOT ${THIRDPARTY_DEPS_DIR})
    set(Protobuf_USE_STATIC_LIBS ON)
    message(STATUS "Using THIRDPARTY_DEPS_DIR=${THIRDPARTY_DEPS_DIR}")
else()
    message(WARNING "deps/ 未就绪，运行 ./deps.py sync 下载三方依赖（头文件/静态库）")
endif()

# SDK 1.7.0 起 lib/ 下有 libchen.so / libchen.so.1 / libchen.so.1.6.0 三个文件（SDK zip 里是三个
# 等大副本，不是链接）。SDK 自带的 CMakeLists 用 file(GLOB lib/*.so) 逐个 add_library(IMPORTED)，
# 三个文件会算出同一个目标名 chen，configure 阶段直接报 “another target with the same name
# already exists”。所以这里不再 add_subdirectory(SDK)，改为显式导入真正的库文件。
set(CHEN_LIBRARY ${CHEN_SDK_DIR}/lib/libchen.so)
if (NOT EXISTS "${CHEN_LIBRARY}")
    message(FATAL_ERROR "找不到 ${CHEN_LIBRARY}，请先执行 ./deps.py sync")
endif ()
if (NOT TARGET chen)
    add_library(chen SHARED IMPORTED GLOBAL)
    set_target_properties(chen PROPERTIES
            IMPORTED_LOCATION ${CHEN_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${CHEN_SDK_DIR}/include
    )
endif ()

add_subdirectory(${PROJECT_SOURCE_DIR}/dbproxy/data)
