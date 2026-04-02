# 项目基本信息
cmake_minimum_required(VERSION 3.18)
project(Rendering VERSION 1.1.0 LANGUAGES CXX C CUDA)

# 全局编译选项
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_RUNTIME_LIBRARY Shared)

# Windows 特定定义
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

# 外部依赖查找（统一管理）
find_package(CUDA REQUIRED)
find_package(CUDAToolkit REQUIRED)
find_package(assimp REQUIRED)
find_package(flatbuffers REQUIRED)

# 第三方库路径变量
set(VIDEO_CODEC_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asset/thirdParty/Video_Codec_SDK_12.0.16")
set(CAD_DATA_MANAGER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asset/thirdParty/CADDataManager")

# 构建核心渲染库
add_subdirectory(asset)

# 主可执行文件
aux_source_directory(src SRCS)
add_executable(Rendering ${SRCS})

target_include_directories(Rendering PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CAD_DATA_MANAGER_DIR}/include
    ${VIDEO_CODEC_SDK_DIR}/Utils
    ${CUDA_INCLUDE_DIRS}
)

# 平台特定链接配置
if(UNIX)
    target_link_directories(Rendering PRIVATE
        ${VIDEO_CODEC_SDK_DIR}/Lib/linux/stubs/x86_64
        ${CAD_DATA_MANAGER_DIR}/lib
    )

    target_link_libraries(Rendering PRIVATE
        vsgRenderer
        vsgImGui::vsgImGui
        vsgXchange::vsgXchange
        flatbuffers::flatbuffers
        nvcuvid
        nvidia-encode
        CUDA::cudart
        ${ASSIMP_LIBRARIES}
    )
elseif(WIN32)
    target_link_libraries(Rendering PRIVATE
        vsgRenderer
        ${CAD_DATA_MANAGER_DIR}/winlib/cadDataManager.lib
    )

    # 复制 DLL 到输出目录
    add_custom_command(TARGET Rendering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CAD_DATA_MANAGER_DIR}/winlib/cadDataManager.dll"
            $<TARGET_FILE_DIR:Rendering>
    )
endif()

# 线程支持
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
