cmake_minimum_required(VERSION 3.23)
project(plugIN-upterm VERSION 1.00.2001)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(ARCH "x86" CACHE STRING "target archecture")

# Check and Parse plugin_key
set(PLUGIN_KEY ${CMAKE_SOURCE_DIR}/plugin_key_${ARCH}.txt CACHE STRING "plugIN key file")
message("Finding ${PLUGIN_KEY}")
if(NOT EXISTS ${PLUGIN_KEY})
    message(FATAL_ERROR "Finding ${PLUGIN_KEY} - not found")
else()
    message("Finding ${PLUGIN_KEY} - found")
endif()
execute_process (
    COMMAND bash -c "echo $(cat ${PLUGIN_KEY}) | awk -F'|' '{printf $1}'" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE PLUGIN_APP_GUID
)
execute_process (
    COMMAND bash -c "echo $(cat ${PLUGIN_KEY}) | awk -F'|' '{printf $2}'" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE PLUGIN_ACCESS_KEY
)

# Check plugin package materials
set(PLUGIN_MATERIAL_DIR ${CMAKE_SOURCE_DIR}/installed_dir)
set(INSTALL_SH_NAME "install_plugIN.sh")
set(PLUGIN_MATERIAL_INSTALL_SH ${PLUGIN_MATERIAL_DIR}/${INSTALL_SH_NAME}) 
message("Finding ${PLUGIN_MATERIAL_INSTALL_SH}")
if(NOT EXISTS ${PLUGIN_MATERIAL_INSTALL_SH})
    message(FATAL_ERROR "Finding ${PLUGIN_MATERIAL_INSTALL_SH} - not found")
else()
    message("Finding ${PLUGIN_MATERIAL_INSTALL_SH} - found")
endif()


option(BUILD_FROM_SDK_SRC "Build from scratch" OFF)
set(ROOT_INSTALL_DIR "/opt/allxon/plugIN" CACHE STRING "PlugIN install root directory")
set(PLUGIN_INSTALL_DIR "${ROOT_INSTALL_DIR}/${PLUGIN_APP_GUID}" CACHE STRING "PlugIN install directory")
set(PLUGIN_NAME ${CMAKE_PROJECT_NAME})
set(PLUGIN_VERSION ${PROJECT_VERSION})
message("=================== CMAKE_DIR : ${CMAKE_SOURCE_DIR} ======================")
message("PLUGIN_NAME: ${PLUGIN_NAME}")
message("PLUGIN_VERSION: ${PLUGIN_VERSION}")
message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("*** PROJECT_DIR : ${PROJECT_SOURCE_DIR} ")
message("PLUGIN_APP_GUID: ${PLUGIN_APP_GUID}")
message("PLUGIN_ACCESS_KEY: ${PLUGIN_ACCESS_KEY}")
message("PLUGIN_INSTALL_DIR: ${PLUGIN_INSTALL_DIR}")
message("BUILD_FROM_SDK_SRC: ${BUILD_FROM_SDK_SRC}")

file(MAKE_DIRECTORY ${ROOT_INSTALL_DIR})

configure_file(build_info.h.in build_info.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# build hello src
aux_source_directory(${PROJECT_SOURCE_DIR}/src SOURCE_FILE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/Util UTIL_SOURCE_FILE)
add_executable(${CMAKE_PROJECT_NAME} ${SOURCE_FILE} ${UTIL_SOURCE_FILE})

if(BUILD_FROM_SDK_SRC)
    add_subdirectory(dep/linux-plugin-sdk)
    add_dependencies(${CMAKE_PROJECT_NAME} plugIN_SDK) 
endif()

if(WIN32)
    target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/dep/linux-plugin-sdk/lib/plugIN_SDK.lib)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC -pthread -lrt -lcrypto -lssl
                        ${PROJECT_SOURCE_DIR}/dep/linux-plugin-sdk/lib/libplugIN_SDK.a)
endif()

list(APPEND EXTRA_INCLUDES ${PROJECT_SOURCE_DIR}/dep/linux-plugin-sdk/include 
                           ${PROJECT_SOURCE_DIR}/dep/websocketpp
                           ${PROJECT_SOURCE_DIR}/dep/asio/asio/include) 

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC 
                            ${EXTRA_INCLUDES}
                            )

# install stage
install(DIRECTORY ${PLUGIN_MATERIAL_DIR}/ DESTINATION ./${PLUGIN_APP_GUID} 
        FILE_PERMISSIONS OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE OWNER_READ GROUP_READ WORLD_READ OWNER_WRITE
        PATTERN ${INSTALL_SH_NAME} EXCLUDE)
install(PROGRAMS ${PLUGIN_MATERIAL_INSTALL_SH} DESTINATION .)
install(PROGRAMS ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME} DESTINATION ./${PLUGIN_APP_GUID})

# package stage
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ")
endif()

set(CPACK_PACKAGE_FILE_NAME ${PLUGIN_APP_GUID})
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)
