aux_source_directory(src assetSRCS)

add_library(vsgRenderer SHARED ${assetSRCS})
set(render_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/)
set_target_properties(vsgRenderer PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${render_LIB_PATH})

target_include_directories(vsgRenderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Utils
    ${CUDA_INCLUDE_DIRS}
)