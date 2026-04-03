cmake_minimum_required(VERSION 3.18)
project(Rendering VERSION 1.1.0 LANGUAGES CXX C CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_RUNTIME_LIBRARY Shared)

set(PROJECT_BUILD_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BUILD_OUTPUT_DIR}")
if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${config}" config_upper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${PROJECT_BUILD_OUTPUT_DIR}")
    endforeach()
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

find_package(CUDA REQUIRED)
find_package(CUDAToolkit REQUIRED)
find_package(assimp REQUIRED)
find_package(flatbuffers REQUIRED)

set(VIDEO_CODEC_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asset/thirdParty/Video_Codec_SDK_12.0.16")
set(CAD_DATA_MANAGER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asset/thirdParty/CADDataManager")
set(CAD_DATA_MANAGER_WIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asset/thirdParty/cadDataConversionTest/thirdparty")

add_subdirectory(asset)

aux_source_directory(src SRCS)
add_executable(Rendering ${SRCS})

target_include_directories(Rendering PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${VIDEO_CODEC_SDK_DIR}/Utils
    ${CUDA_INCLUDE_DIRS}
)

if(WIN32)
    target_include_directories(Rendering PRIVATE
        ${CAD_DATA_MANAGER_WIN_DIR}/include
        ${CAD_DATA_MANAGER_WIN_DIR}/include/cadDataManager
    )
else()
    target_include_directories(Rendering PRIVATE
        ${CAD_DATA_MANAGER_DIR}/include
    )
endif()

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

    add_custom_command(TARGET Rendering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CAD_DATA_MANAGER_WIN_DIR}/libs/Windows/cadDataManager.dll"
            $<TARGET_FILE_DIR:Rendering>
    )
endif()

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()
