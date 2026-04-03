cmake_minimum_required(VERSION 3.10)
project(cadDataConversionTest)
set(CMAKE_CXX_STANDARD 17)

include_directories(thirdparty/include)
include_directories(thirdparty/include/cadDataManager)

add_executable(cadDataConversionTest main.cpp)

if(WIN32)
	target_link_libraries (
		cadDataConversionTest 
		PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libs/Windows/cadDataManager.lib
	)
	add_custom_command(
		TARGET cadDataConversionTest POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libs/Windows/cadDataManager.dll
		$<TARGET_FILE_DIR:cadDataConversionTest>
		COMMENT "Copying cadDataManager.dll to output directory"
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries (
		cadDataConversionTest 
		PRIVATE 
		${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libs/Linux/libcadDataManager.so
	)
	target_link_libraries (cadDataConversionTest PRIVATE pthread)
elseif(ANDROID)
	target_link_libraries (
		cadDataConversionTest 
		PRIVATE 
		${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libs/Android/cadDataManager.so
	)
endif()


set(JSON_FILES init.json)
foreach(json ${JSON_FILES})
	get_filename_component(json_src ${json} ABSOLUTE)

	add_custom_command(
		TARGET cadDataConversionTest POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${json_src}
		$<TARGET_FILE_DIR:${PROJECT_NAME}>
		COMMENT "Copying init.json to output directory"
		VERBATIM
	)
endforeach()
