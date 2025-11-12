aux_source_directory(src assetSRCS)
if(UNIX)
    add_subdirectory(thirdParty/VulkanSceneGraph)
    add_subdirectory(thirdParty/vsgImGui)
    add_subdirectory(thirdParty/vsgXchange)

    add_library(vsgRenderer SHARED ${assetSRCS})
    set(render_LIB_PATH 
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/
    )
    set_target_properties(vsgRenderer PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${render_LIB_PATH})

    target_link_libraries(vsgRenderer
        flatbuffers
        vsg::vsg
        vsgImGui::vsgImGui
        libnvcuvid.so libnvidia-encode.so ${CUDA_CUDA_LIBRARY}
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/CADDataManager/lib/cadDataManager.a 
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/CADDataManager/lib/libcurl.a
    )

    target_include_directories(vsgRenderer PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/CADDataManager/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Utils
        ${CUDA_INCLUDE_DIRS}
    )

    target_link_libraries(vsgRenderer vsg::vsg)
elseif(WIN32)
    add_subdirectory(thirdParty/VulkanSceneGraph)
    add_subdirectory(thirdParty/vsgImGui)
    find_package(assimp REQUIRED)
    add_subdirectory(thirdParty/vsgXchange)
    find_package(flatbuffers REQUIRED)
    find_package(CUDA REQUIRED)

    add_library(vsgRenderer ${assetSRCS})
    set(render_LIB_PATH 
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/
    )
    set_target_properties(vsgRenderer PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${render_LIB_PATH})

    target_link_libraries(vsgRenderer PUBLIC
        flatbuffers::flatbuffers
        vsgXchange::vsgXchange
        assimp::assimp
        vsgImGui::vsgImGui         
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Lib/x64/nvcuvid.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Lib/x64/nvencodeapi.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/cadDataManager/winlib/cadDataManager.lib 
        ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY}
    )

    target_include_directories(vsgRenderer PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/cadDataManager/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/Video_Codec_SDK_12.0.16/Utils
        ${CUDA_INCLUDE_DIRS}
        ${ASSIMP_INCLUDE_DIR}
    )
endif()

