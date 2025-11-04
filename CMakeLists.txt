cmake_minimum_required(VERSION 3.10)
project(gpu_monitor_service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- スレッド ---
find_package(Threads REQUIRED)

# --- Boostライブラリ ---
find_package(Boost COMPONENTS system json REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# --- NVML (NVIDIA Management Library) ---
# 【重要】WSLの特殊なパスを明示的に指定してライブラリを検索
find_library(NVML_LIBRARY nvml PATHS /usr/lib/x86_64-linux-gnu /usr/lib/nvidia)
if(NOT NVML_LIBRARY)
    message(FATAL_ERROR "NVML library not found. Install NVIDIA CUDA Toolkit.")
endif()

# --- GPU監視API (メインターゲット) ---
add_executable(gpu_monitor src/gpu_monitor.cpp)

# NVMLヘッダーの場所もインクルード (nvml.hを見つけるため)
target_include_directories(gpu_monitor PRIVATE 
    /usr/include/nvidia
    /usr/include
)

# ライブラリのリンク
target_link_libraries(gpu_monitor PRIVATE
    Boost::system
    Boost::json
    ${NVML_LIBRARY}  
    Threads::Threads
    pthread
)

# --- 出力先設定 ---
set_target_properties(gpu_monitor PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)