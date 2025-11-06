cmake_minimum_required(VERSION 3.13)

project(qtrainsim LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Gui Test Widgets PrintSupport)
if (NOT Qt5_FOUND)
    find_package(Qt6 COMPONENTS Core Gui Test Widgets PrintSupport REQUIRED)
endif()

if (Qt5_FOUND)
    add_definitions(-DUSING_QT5)
else()
    add_definitions(-DUSING_QT6)
endif()

file(GLOB HEADER_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/*.h
)

file(GLOB SOURCE_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
)

add_library(qtrainsim STATIC ${SOURCE_FILES} ${HEADER_FILES})

if (Qt5_FOUND)
    target_link_libraries(qtrainsim PUBLIC Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Test Qt5::PrintSupport)
else()
    target_link_libraries(qtrainsim PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::PrintSupport)
endif()

target_include_directories(qtrainsim PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)

file(COPY data/ DESTINATION ${CMAKE_BINARY_DIR}/data)
