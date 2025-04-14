aux_source_directory(src assetSRCS)
if(UNIX)
    find_package(vsg REQUIRED)
    find_package(vsgXchange REQUIRED)

    add_library(vsgRenderer SHARED ${assetSRCS})
    set(render_LIB_PATH 
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/
    )
    set_target_properties(vsgRenderer PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${render_LIB_PATH})

    target_link_libraries(vsgRenderer
        flatbuffers
        vsg::vsg
        vsgImGui::vsgImGui
        libnvcuvid.so libnvidia-encode.so ${CUDA_CUDA_LIBRARY} ${ASSIMP_LIBRARIES}
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/CADDataManager/lib/cadDataManager.a 
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/CADDataManager/lib/libcurl.a
    )

    target_include_directories(vsgRenderer PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/CADDataManager/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Utils
        ${CUDA_INCLUDE_DIRS}
        ${ASSIMP_INCLUDE_DIR}
    )

    target_link_libraries(vsgRenderer vsg::vsg vsgXchange::vsgXchange)
elseif(WIN32)
    find_package(vsg REQUIRED)
    find_package(CUDA REQUIRED)
    message(STATUS "assetSRCS: ${assetSRCS}")
    add_library(vsgRenderer ${assetSRCS})
    set(render_LIB_PATH 
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/
    )
    set_target_properties(vsgRenderer PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${render_LIB_PATH})

    target_link_directories(vsgRenderer PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Lib/linux/stubs/x86_64
            ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Lib/x64
            ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/cadDataManager/lib
    )

    target_link_libraries(vsgRenderer PUBLIC
        vsg::vsg
        nvcuvid nvencodeapi ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY}
        cadDataManager
        curl
        "C:/Program Files/Assimp/lib/x64/assimp-vc143-mt.lib"
    )

    target_include_directories(vsgRenderer PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/cadDataManager/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Utils
        ${CUDA_INCLUDE_DIRS}
        "C:/Program Files/Assimp/include"
    )
endif()

