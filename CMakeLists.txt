cmake_minimum_required(VERSION 3.7)

project(Rendering
        VERSION 1.1.0
        DESCRIPTION "Set of example programs that test and illustrate how to use the VulkanSceneGraph"
        LANGUAGES CXX C
)

find_package(CUDA REQUIRED)
# set(CMAKE_CUDA_ARCHITECTURES 61)

# find the vsg
if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
    set(ENV{VK_SDK_PATH} ${VULKAN_SDK})
endif()


# set the use of C++17 globally as all examples require it
set(CMAKE_CXX_STANDARD 17)
aux_source_directory(src SRCS)

add_executable(Rendering ${SRCS})
add_subdirectory(asset)
add_subdirectory(thirdParty/vsg)
add_subdirectory(thirdParty/vsgImGui)

set(ASSIMP_INSTALL FALSE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT FALSE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT FALSE)
set(ASSIMP_BUILD_PLY_IMPORTER TRUE)
set(ASSIMP_BUILD_OBJ_IMPORTER TRUE)
set(ASSIMP_BUILD_PLY_EXPORTER TRUE)
set(ASSIMP_BUILD_TESTS FALSE)
add_subdirectory(thirdParty/assimp)

target_include_directories(Rendering PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_directories(Rendering PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Lib/linux/stubs/x86_64
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/FlatBuffers/include
    ${CMAKE_SOURCE_DIR}/thirdParty/stb_image/include
    ${CMAKE_SOURCE_DIR}/thirdParty/vsgImGui/include
    ${CMAKE_SOURCE_DIR}/thirdParty/assimp/include
)

target_link_libraries(Rendering 
    vsg::vsg 
    flatbuffers
    vsgRenderer
    libnvcuvid.so libnvidia-encode.so ${CUDA_CUDA_LIBRARY})

get_target_property(Rendering_include_dirs Rendering INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "Rendering include directories:${Rendering_include_dirs}")

