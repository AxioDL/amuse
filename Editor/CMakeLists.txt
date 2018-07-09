cmake_minimum_required(VERSION 3.10)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(AUTORCC ON)

find_package(Qt5Widgets)
find_package(Qt5Network)
find_package(Qt5Xml)

if(WIN32)
  list(APPEND PLAT_SRCS platforms/win/amuse-gui.rc platforms/win/amuse-gui.manifest)
elseif(APPLE)
  list(APPEND PLAT_SRCS platforms/mac/mainicon.icns MacOSExtras.mm)
  find_library(APPKIT_LIBRARY AppKit)
  list(APPEND PLAT_LIBS ${APPKIT_LIBRARY})
  set_source_files_properties(platforms/mac/mainicon.icns PROPERTIES
          MACOSX_PACKAGE_LOCATION Resources)
endif()

add_subdirectory(platforms/freedesktop)
declare_qticon_target()
list(APPEND PLAT_SRCS mainicon_qt.cpp)

QT5_ADD_RESOURCES(qrc_resources.cpp resources/resources.qrc)

add_executable(amuse-gui WIN32 MACOSX_BUNDLE
        MainWindow.ui MainWindow.hpp MainWindow.cpp
        KeyboardWidget.hpp KeyboardWidget.cpp
        StatusBarWidget.hpp StatusBarWidget.cpp
        ProjectModel.hpp ProjectModel.cpp
        ProjectStatistics.hpp ProjectStatistics.cpp
        SoundMacroEditor.hpp SoundMacroEditor.cpp
        KeymapEditor.hpp KeymapEditor.cpp
        LayersEditor.hpp LayersEditor.cpp
        SampleEditor.hpp SampleEditor.cpp
        SoundGroupEditor.hpp SoundGroupEditor.cpp
        SongGroupEditor.hpp SongGroupEditor.cpp
        resources/resources.qrc qrc_resources.cpp
        ${PLAT_SRCS}
        main.cpp)

set_target_properties(amuse-gui PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platforms/mac/Info.plist")

target_link_libraries(amuse-gui ${PLAT_LIBS}
        ${Qt5Widgets_LIBRARIES}
        ${Qt5Network_LIBRARIES}
        ${Qt5Xml_LIBRARIES}
        boo logvisor zeus athena-core athena-libyaml xxhash z)
