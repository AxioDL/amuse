cmake_minimum_required(VERSION 3.10)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5Widgets)
find_package(Qt5Network)
find_package(Qt5Xml)

add_subdirectory(platforms/freedesktop)
declare_qticon_target()
list(APPEND PLAT_SRCS mainicon_qt.cpp)

add_executable(amuse-gui WIN32 MACOSX_BUNDLE
        MainWindow.ui MainWindow.hpp MainWindow.cpp
        ${PLAT_SRCS}
        main.cpp)

set_target_properties(amuse-gui PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platforms/mac/Info.plist")

target_link_libraries(amuse-gui ${PLAT_LIBS}
        ${Qt5Widgets_LIBRARIES}
        ${Qt5Network_LIBRARIES}
        ${Qt5Xml_LIBRARIES}
        boo logvisor zeus athena-core athena-libyaml xxhash z)
