cmake_minimum_required(VERSION 3.17)
project(grazemate LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# godot-cpp as subdirectory
add_subdirectory(godot-cpp)

# Collect sources
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)

# Build for template_debug target alias
foreach(TARGET_ALIAS template_debug template_release editor)
    set(TARGET_NAME "grazemate.${TARGET_ALIAS}")

    add_library(${TARGET_NAME} SHARED)

    target_sources(${TARGET_NAME} PRIVATE ${SOURCES})

    target_include_directories(${TARGET_NAME} PRIVATE src)

    set(LINK_TARGET "godot-cpp::${TARGET_ALIAS}")
    target_link_libraries(${TARGET_NAME} PRIVATE ${LINK_TARGET})

    get_target_property(GODOTCPP_SUFFIX ${LINK_TARGET} GODOTCPP_SUFFIX)
    get_target_property(OSX_ARCH ${LINK_TARGET} OSX_ARCHITECTURES)

    set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/project/bin/")

    set_target_properties(${TARGET_NAME}
        PROPERTIES
        CXX_STANDARD 17
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH_USE_ORIGIN ON
        LINK_SEARCH_START_STATIC ON
        LINK_SEARCH_END_STATIC ON
        LIBRARY_OUTPUT_DIRECTORY "$<1:${OUTPUT_DIR}>"
        RUNTIME_OUTPUT_DIRECTORY "$<1:${OUTPUT_DIR}>"
        PREFIX "lib"
        OUTPUT_NAME "grazemate${GODOTCPP_SUFFIX}"
        FOLDER "grazemate"
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set_target_properties(${TARGET_NAME}
            PROPERTIES
            PREFIX ""
            SUFFIX ""
            OUTPUT_NAME "libgrazemate.macos.${TARGET_ALIAS}.framework"
            OSX_ARCHITECTURES "${OSX_ARCH}"
        )
    endif()
endforeach()
