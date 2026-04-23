
cmake_minimum_required(VERSION 3.5...4.0)

project(dx12)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)


add_library(volk STATIC lib/volk.c)
target_include_directories(volk PUBLIC include)


add_library(SDL3 SHARED IMPORTED)

set_target_properties(SDL3 PROPERTIES
	IMPORTED_IMPLIB "${CMAKE_SOURCE_DIR}/lib/SDL3.lib"
	IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/lib/SDL3.dll"
	)

add_executable(main
 src/main.cpp
 src/VkBootstrap.cpp
 src/vma.cpp
 src/cgltf.cpp)

target_include_directories(main PRIVATE include)

target_link_libraries(main SDL3 volk)

# set_target_properties(main PROPERTIES WIN32_EXECUTABLE TRUE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(main PRIVATE DEBUG_BUILD)
endif()

add_custom_command(TARGET main POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	"${CMAKE_SOURCE_DIR}/lib/SDL3.dll"
	"$<TARGET_FILE_DIR:main>"
	)