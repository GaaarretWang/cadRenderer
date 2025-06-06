cmake_minimum_required(VERSION 3.7)

project(Rendering
        VERSION 1.1.0
        DESCRIPTION "Set of example programs that test and illustrate how to use the VulkanSceneGraph"
        LANGUAGES CXX C CUDA
)

# set_property(TARGET tgt PROPERTY CUDA_ARCHITECTURES native)
# set(CMAKE_CUDA_ARCHITECTURES tgt)
find_package(CUDA REQUIRED)

#find_package(vsg REQUIRED)

find_package(vsg 1.1.0 REQUIRED)

find_package(vsgXchange REQUIRED)

find_package(assimp REQUIRED)

find_package(vsgImGui REQUIRED)

# set the use of C++17 globally as all examples require it
set(CMAKE_CXX_STANDARD 17)
aux_source_directory(src SRCS)

add_executable(Rendering ${SRCS})
add_subdirectory(asset)

target_include_directories(Rendering PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/asset/thirdParty/CADDataManager/include
)

target_link_directories(Rendering PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Lib/linux/stubs/x86_64
    ${CMAKE_CURRENT_SOURCE_DIR}/asset/thirdParty/CADDataManager/lib/
)

target_link_libraries (${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/asset/thirdParty/CADDataManager/lib/cadDataManager.a ${CMAKE_CURRENT_SOURCE_DIR}/asset/thirdParty/CADDataManager/lib/libcurl.a)

target_link_libraries(Rendering 
    vsg::vsg
    vsgImGui::vsgImGui
    vsgXchange::vsgXchange
    flatbuffers
    vsgRenderer
    libnvcuvid.so libnvidia-encode.so ${CUDA_CUDA_LIBRARY}
    ${ASSIMP_LIBRARIES}
    )

get_target_property(Rendering_include_dirs Rendering INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "Rendering include directories:${Rendering_include_dirs}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}-pthread")
