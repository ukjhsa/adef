cmake_minimum_required(VERSION 3.15)

project(adef
        VERSION 1.0.0
        DESCRIPTION "A Software Framework for Adaptive Differential Evolution"
        LANGUAGES CXX)

option(GENERATE_EXECUTABLE "Generate the executable file" ON)
option(BUILD_SHARED_LIBS "Build adef as the shared library" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

include(GNUInstallDirs)
include(CTest)

set(ADEF_INCLUDE_DIR include)
set(JSON_INCLUDE_DIR thirdparty/json)


# build libadef
add_subdirectory(src)

# generate executable file
if(GENERATE_EXECUTABLE)
    set(main_source src/main.cpp)
    add_executable(adef ${main_source})

    # link to libadef
    target_link_libraries(adef PRIVATE adef::adef)

    set_target_properties(adef PROPERTIES
                          OUTPUT_NAME adef
                          RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
                          CXX_EXTENSIONS OFF)

    # copy the default configuration file
    add_custom_command(TARGET adef POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/config.json"
            "$<TARGET_FILE_DIR:adef>/config.json"
        COMMENT "Copying default config.json to executable directory")

    install(TARGETS adef
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    if(BUILD_TESTING)
        add_test(NAME adef_default_config
                 COMMAND adef -file "${PROJECT_SOURCE_DIR}/config.json"
                 WORKING_DIRECTORY $<TARGET_FILE_DIR:adef>)
    endif()
endif()
