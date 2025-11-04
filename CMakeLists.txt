cmake_minimum_required(VERSION 3.10)
project(gpu_monitor_service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Threads ---
find_package(Threads REQUIRED)

# --- Boost (system, json, program_options) ---
find_package(Boost COMPONENTS system json program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# --- NVML (NVIDIA Management Library) ---
find_library(NVML_LIBRARY nvml
    PATHS
        /usr/lib/x86_64-linux-gnu
        /usr/lib/nvidia
        /usr/lib/wsl/lib
        /usr/local/cuda/lib64/stubs
)

if(NOT NVML_LIBRARY)
    message(WARNING "NVML library not found automatically. Trying default path...")
    set(NVML_LIBRARY "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so")
endif()

if(NOT EXISTS ${NVML_LIBRARY})
    message(FATAL_ERROR "NVML library not found. Please install NVIDIA CUDA Toolkit or check your driver.")
endif()

message(STATUS "✅ NVML library found at: ${NVML_LIBRARY}")

# --- Target: gpu_monitor ---
add_executable(gpu_monitor src/gpu_monitor.cpp)

# NVMLヘッダーを探索
target_include_directories(gpu_monitor PRIVATE
    /usr/include/nvidia
    /usr/include
)

# --- リンク ---
target_link_libraries(gpu_monitor PRIVATE
    Boost::system
    Boost::json
    Threads::Threads
    ${NVML_LIBRARY}
    pthread
)

# --- 出力先 ---
set_target_properties(gpu_monitor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# --- ccache 有効化 ---
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()
