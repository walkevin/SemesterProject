cmake_minimum_required(VERSION 2.6)
project(505_MIQ)

include("../../CMakeLists.shared")

find_package(LIBCOMISO REQUIRED)
find_package(GMM REQUIRED)
find_package(TINYXML2 REQUIRED)

include_directories( ${LIBCOMISO_INCLUDE_DIRS} )
include_directories( ${GMM_INCLUDE_DIR} )
include_directories( ${TINYXML2_INCLUDE_DIR})

set(SOURCES
${PROJECT_SOURCE_DIR}/main.cpp
)

add_executable(${PROJECT_NAME}_bin ${SOURCES} ${SHARED_SOURCES})
target_link_libraries(${PROJECT_NAME}_bin ${SHARED_LIBRARIES} ${LIBCOMISO_LIBRARY})

