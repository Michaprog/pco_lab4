cmake_minimum_required(VERSION 3.16)
project(PCO_LAB04 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Gui Widgets PrintSupport QUIET)
if (Qt5_FOUND)
    add_definitions(-DUSING_QT5)
else()
    find_package(Qt6 COMPONENTS Core Gui Widgets PrintSupport REQUIRED)
    add_definitions(-DUSING_QT6)
endif()

find_library(QTRAINSIM_LIB qtrainsim PATHS ${CMAKE_CURRENT_BUILD_DIR}/QtrainSim/)

option(WITH_TSAN "Build with ThreadSanitizer" OFF)

add_library(trains_core OBJECT
    src/locomotive.cpp
    src/locomotivebehavior.cpp
    src/sharedsectioninterface.h
    src/locomotive.h
    src/launchable.h
    src/locomotivebehavior.h
    src/sharedsection.h
    ../QtrainSim/qtrainsim.qrc
)

target_include_directories(trains_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if (Qt5_FOUND)
    target_link_libraries(trains_core PUBLIC Qt5::Core Qt5::Gui Qt5::Widgets Qt5::PrintSupport)
else()
    target_link_libraries(trains_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport)
endif()

target_link_libraries(trains_core PUBLIC qtrainsim)

add_executable(pco_lab04 src/cppmain.cpp)
target_link_libraries(pco_lab04 PRIVATE trains_core -lpcosynchro)
target_sources(pco_lab04 PRIVATE $<TARGET_OBJECTS:trains_core>)
file(COPY ../QtrainSim/data DESTINATION ${CMAKE_BINARY_DIR}/code)


enable_testing()
find_package(GTest REQUIRED)
add_executable(unit_tests
    tests/main.cpp
)

target_include_directories(unit_tests BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(unit_tests PRIVATE USE_FAKE_LOCO)

if (Qt5_FOUND)
    target_link_libraries(unit_tests PRIVATE Qt5::Core)
else()
    target_link_libraries(unit_tests PRIVATE Qt6::Core)
endif()


target_link_libraries(unit_tests PRIVATE
    GTest::gtest GTest::gtest_main
    -lpcosynchro
)

if (WITH_TSAN)
    target_compile_options(unit_tests PRIVATE -fsanitize=thread)
    target_link_options(unit_tests PRIVATE -fsanitize=thread)
    target_compile_options(pco_lab04 PRIVATE -fsanitize=thread)
    target_link_options(pco_lab04 PRIVATE -fsanitize=thread)
endif()

add_test(NAME unit_tests COMMAND unit_tests)