# 收集源文件
aux_source_directory(src ASSET_SRCS)

# 构建 VulkanSceneGraph 生态系统
add_subdirectory(thirdParty/VulkanSceneGraph)
add_subdirectory(thirdParty/vsgImGui)
add_subdirectory(thirdParty/vsgXchange)

# 创建渲染库
if(UNIX)
    add_library(vsgRenderer SHARED ${ASSET_SRCS})
else()
    add_library(vsgRenderer STATIC ${ASSET_SRCS})
endif()

# 输出目录
set_target_properties(vsgRenderer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib"
)

# 公共头文件路径
target_include_directories(vsgRenderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 私有头文件路径
target_include_directories(vsgRenderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/vsgXchange/src/stbi
    ${CAD_DATA_MANAGER_DIR}/include
    ${VIDEO_CODEC_SDK_DIR}/Utils
    ${CUDA_INCLUDE_DIRS}
)

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
        ${CUDA_CUDA_LIBRARY}
        ${CAD_DATA_MANAGER_DIR}/lib/libcadDataManager.so
        ${CAD_DATA_MANAGER_DIR}/lib/libcurl.a
    )
elseif(WIN32)
    target_link_libraries(vsgRenderer PRIVATE
        flatbuffers::flatbuffers
        assimp::assimp
        ${VIDEO_CODEC_SDK_DIR}/Lib/x64/nvcuvid.lib
        ${VIDEO_CODEC_SDK_DIR}/Lib/x64/nvencodeapi.lib
        ${CAD_DATA_MANAGER_DIR}/winlib/cadDataManager.lib
        ${CUDA_LIBRARIES}
        ${CUDA_CUDA_LIBRARY}
    )

    target_include_directories(vsgRenderer PRIVATE
        ${ASSIMP_INCLUDE_DIR}
    )
endif()

