aux_source_directory(src assetSRCS)

find_package(vsg REQUIRED)

add_library(vsgRenderer SHARED ${assetSRCS})
set(render_LIB_PATH 
	${CMAKE_CURRENT_SOURCE_DIR}/lib/
)
set_target_properties(vsgRenderer PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${render_LIB_PATH})

target_link_libraries(vsgRenderer
    flatbuffers
    vsg::vsg
    libnvcuvid.so libnvidia-encode.so ${CUDA_CUDA_LIBRARY}
)

target_include_directories(vsgRenderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/CADDataManager/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Utils
    ${CUDA_INCLUDE_DIRS}
)

target_link_libraries(vsgRenderer vsg::vsg )
