cmake_minimum_required(VERSION 3.23)
project(plugIN-upterm VERSION 1.00.2001)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(ARCH "x86" CACHE STRING "target archecture")

# Check and Parse plugin_key
set(PLUGIN_KEY ${CMAKE_SOURCE_DIR}/plugin_key_${ARCH}.json CACHE FILEPATH "plugIN key file")
file(READ ${PLUGIN_KEY} PLUGIN_KEY_CONTENT)
message(${PLUGIN_KEY_CONTENT})
string(JSON PLUGIN_APP_GUID GET ${PLUGIN_KEY_CONTENT} app_guid)
string(JSON PLUGIN_ACCESS_KEY GET ${PLUGIN_KEY_CONTENT} access_key)

# Check plugin package materials
set(PLUGIN_MATERIAL_DIR ${CMAKE_SOURCE_DIR}/installed_dir)
set(INSTALL_SH_NAME "install_plugIN.sh")
set(PLUGIN_MATERIAL_INSTALL_SH ${PLUGIN_MATERIAL_DIR}/${INSTALL_SH_NAME}) 
message(CHECK_START "Finding ${PLUGIN_MATERIAL_INSTALL_SH}")
if(NOT EXISTS ${PLUGIN_MATERIAL_INSTALL_SH})
    message(CHECK_FAIL "not found")
else()
    message(CHECK_PASS "found")
endif()

option(BUILD_FROM_SDK_SRC "Build from scratch" OFF)
set(PLUGIN_NAME ${CMAKE_PROJECT_NAME})
set(PLUGIN_VERSION ${PROJECT_VERSION})
message("=================== CMAKE_DIR : ${CMAKE_SOURCE_DIR} ======================")
message("PLUGIN_NAME: ${PLUGIN_NAME}")
message("PLUGIN_VERSION: ${PLUGIN_VERSION}")
message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("*** PROJECT_DIR : ${PROJECT_SOURCE_DIR} ")
message("PLUGIN_APP_GUID: ${PLUGIN_APP_GUID}")
message("PLUGIN_ACCESS_KEY: ${PLUGIN_ACCESS_KEY}")
message("BUILD_FROM_SDK_SRC: ${BUILD_FROM_SDK_SRC}")

configure_file(build_info.h.in build_info.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# build hello src
aux_source_directory(${PROJECT_SOURCE_DIR}/src SOURCE_FILE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/Util UTIL_SOURCE_FILE)
add_executable(${CMAKE_PROJECT_NAME} ${SOURCE_FILE} ${UTIL_SOURCE_FILE})

if(BUILD_FROM_SDK_SRC)
    set(PLUGIN_SDK_TARGET plugIN_SDK)
    add_subdirectory(dep/linux-plugin-sdk)
    add_dependencies(${CMAKE_PROJECT_NAME} ${PLUGIN_SDK_TARGET})
endif()

list(APPEND LINK_LIBS ${PLUGIN_SDK_TARGET} -pthread -lrt -lcrypto -lssl)
list(APPEND EXTRA_INCLUDES ${PROJECT_SOURCE_DIR}/dep/linux-plugin-sdk/include 
                           ${PROJECT_SOURCE_DIR}/dep/websocketpp
                           ${PROJECT_SOURCE_DIR}/dep/asio/asio/include) 

add_compile_definitions("$<$<CONFIG:DEBUG>:DEBUG>")
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${LINK_LIBS})
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
