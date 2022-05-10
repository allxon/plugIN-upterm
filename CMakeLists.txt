cmake_minimum_required(VERSION 3.10)
message("====================== WORKING DIR : ${PROJECT_SOURCE_DIR} ======================")

project(plugIN-upterm VERSION 1.00.2001)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(BUILD_FROM_SDK_SRC "Build from scratch" OFF)
SET(ARCH "x86" CACHE STRING "target archecture")

# Parse plugin_key
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/plugin_key_${ARCH}")
    message(FATAL_ERROR "plugin_key not found!")
endif()
file(STRINGS plugin_key_${ARCH} PLUGIN_KEY_CONTENT)
string(FIND ${PLUGIN_KEY_CONTENT} "|" KEY_SEPARATE_INDEX)
string(LENGTH ${PLUGIN_KEY_CONTENT} PLUGIN_KEY_CONTENT_SIZE)
math(EXPR APP_GUID_LENGTH "${PLUGIN_KEY_CONTENT_SIZE} - ${KEY_SEPARATE_INDEX}")
string(SUBSTRING ${PLUGIN_KEY_CONTENT} "0" ${APP_GUID_LENGTH} PLUGIN_APP_GUID)
math(EXPR KEY_SEPARATE_INDEX "${KEY_SEPARATE_INDEX} + 1")
string(SUBSTRING ${PLUGIN_KEY_CONTENT} ${KEY_SEPARATE_INDEX} -1 PLUGIN_ACCESS_KEY)

message("${CMAKE_PROJECT_NAME} VERSION: ${CMAKE_PROJECT_VERSION}")
message("PLUGIN_APP_GUID: ${PLUGIN_APP_GUID}")
message("PLUGIN_ACCESS_KEY: ${PLUGIN_ACCESS_KEY}")

configure_file(build_info.h.in build_info.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

############### import libs ############### 
# list(APPEND EXTRA_INCLUDES "${PROJECT_SOURCE_DIR}/argon2/include")
# if(WIN32)
#     list(APPEND EXTRA_LIBS "${PROJECT_SOURCE_DIR}/argon2/lib/Argon2OptDll.lib")
#     list(APPEND EXTRA_BINS "${PROJECT_SOURCE_DIR}/argon2/bin/Argon2OptDll.dll")
# endif()
############### compile source files ############### 
# build hello src
aux_source_directory(${PROJECT_SOURCE_DIR}/src SOURCE_FILE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/Util UTIL_SOURCE_FILE)
add_executable(plugIN-upterm ${SOURCE_FILE} ${UTIL_SOURCE_FILE})

if(BUILD_FROM_SDK_SRC)
    #build SDK files
    message("BUILD FROM SDK SRC")
    add_subdirectory(dep/linux-plugin-sdk)
    add_dependencies(plugIN-upterm plugIN_SDK)
endif()

if(WIN32)
    target_link_libraries(plugIN-upterm PUBLIC "${PROJECT_SOURCE_DIR}/dep/linux-plugin-sdk/lib/plugIN_SDK.lib")
elseif(UNIX AND NOT APPLE)
    target_link_libraries(plugIN-upterm PUBLIC -pthread -lrt -lcrypto -lpthread -lboost_system -lboost_chrono -lboost_random -lssl
                        "${PROJECT_SOURCE_DIR}/dep/linux-plugin-sdk/lib/libplugIN_SDK.a")
endif()

list(APPEND EXTRA_INCLUDES "${PROJECT_SOURCE_DIR}/dep/linux-plugin-sdk/include" "${PROJECT_SOURCE_DIR}/dep/websocketpp/include") 

target_include_directories(plugIN-upterm PUBLIC 
                            ${EXTRA_INCLUDES}
                            )

# install stage
install(DIRECTORY installed_dir/ DESTINATION ./${PLUGIN_APP_GUID} USE_SOURCE_PERMISSIONS)
install(PROGRAMS install_plugIN.sh DESTINATION .)

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
