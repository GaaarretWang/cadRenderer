aux_source_directory(src assetSRCS)
message(STATUS "assetSRCS: ${assetSRCS}")

add_library(vsgRenderer SHARED ${assetSRCS})
set(render_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/)
set_target_properties(vsgRenderer PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${render_LIB_PATH})

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/FlatBuffers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdParty/vsg/include
    ${CMAKE_SOURCE_DIR}/thirdParty/stb_image/include
    ${CMAKE_SOURCE_DIR}/thirdParty/vsgImGui/include
    ${CMAKE_SOURCE_DIR}/thirdParty/assimp/include
)

target_link_libraries(vsgRenderer assimp vsg flatbuffers vsgImGui)

target_include_directories(vsgRenderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Utils
    ${CUDA_INCLUDE_DIRS}
)