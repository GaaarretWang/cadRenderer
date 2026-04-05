enable_language(CUDA)
find_package(CUDA REQUIRED)
set(CMAKE_CUDA_RUNTIME_LIBRARY Shared)
find_package(CUDAToolkit REQUIRED)

# 收集源文件
aux_source_directory(src ASSET_SRCS)

# 构建 VulkanSceneGraph 生态系统
add_subdirectory(thirdParty/VulkanSceneGraph)
add_subdirectory(thirdParty/vsgImGui)
add_subdirectory(thirdParty/vsgXchange)

# 创建渲染库
add_library(vsgRenderer STATIC ${ASSET_SRCS})


# 公共头文件路径
target_include_directories(vsgRenderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 私有头文件路径
target_include_directories(vsgRenderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/vsgXchange/src/stbi
    ${VIDEO_CODEC_SDK_DIR}/Utils
    ${CUDAToolkit_INCLUDE_DIRS}
)

if(WIN32)
    target_include_directories(vsgRenderer PRIVATE
        ${CAD_DATA_MANAGER_WIN_DIR}/include
        ${CAD_DATA_MANAGER_WIN_DIR}/include/cadDataManager
    )
else()
    target_include_directories(vsgRenderer PRIVATE
        ${CAD_DATA_MANAGER_DIR}/include
    )
endif()

# 核心依赖（所有平台）
target_link_libraries(vsgRenderer PUBLIC
    vsg::vsg
    vsgImGui::vsgImGui
    vsgXchange::vsgXchange
)

# 平台特定依赖
if(UNIX)
    target_link_libraries(vsgRenderer PRIVATE
        flatbuffers
        nvcuvid
        nvidia-encode
        CUDA::cuda_driver
        ${CAD_DATA_MANAGER_DIR}/lib/libcadDataManager.so
        ${CAD_DATA_MANAGER_DIR}/lib/libcurl.a
    )
elseif(WIN32)
    target_link_libraries(vsgRenderer PRIVATE
        flatbuffers::flatbuffers
        assimp::assimp
        ${VIDEO_CODEC_SDK_DIR}/Lib/x64/nvcuvid.lib
        ${VIDEO_CODEC_SDK_DIR}/Lib/x64/nvencodeapi.lib
        ${CAD_DATA_MANAGER_WIN_DIR}/libs/Windows/cadDataManager.lib
        ${CUDA_LIBRARIES}
        CUDA::cuda_driver
    )

    target_include_directories(vsgRenderer PRIVATE
        ${ASSIMP_INCLUDE_DIR}
    )
endif()
