cmake_minimum_required(VERSION 3.10)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(AUTORCC ON)

find_package(Qt5Widgets)
find_package(Qt5Network)
find_package(Qt5Xml)
find_package(Qt5Svg)

if(WIN32)
  list(APPEND PLAT_SRCS platforms/win/amuse-gui.rc platforms/win/amuse-gui.manifest)
elseif(APPLE)
  list(APPEND PLAT_SRCS platforms/mac/mainicon.icns MacOSExtras.mm)
  set_source_files_properties(platforms/mac/mainicon.icns PROPERTIES
          MACOSX_PACKAGE_LOCATION Resources)
endif()

add_subdirectory(platforms/freedesktop)
declare_qticon_target()
list(APPEND PLAT_SRCS mainicon_qt.cpp)

QT5_ADD_RESOURCES(qrc_resources.cpp resources/resources.qrc)

add_executable(amuse-gui WIN32 MACOSX_BUNDLE
        Common.hpp Common.cpp
        MainWindow.ui MainWindow.hpp MainWindow.cpp
        KeyboardWidget.hpp KeyboardWidget.cpp
        StatusBarWidget.hpp StatusBarWidget.cpp
        ProjectModel.hpp ProjectModel.cpp
        ProjectStatistics.hpp ProjectStatistics.cpp
        EditorWidget.hpp EditorWidget.cpp
        SoundMacroEditor.hpp SoundMacroEditor.cpp
        KeymapEditor.hpp KeymapEditor.cpp
        LayersEditor.hpp LayersEditor.cpp
        SampleEditor.hpp SampleEditor.cpp
        SFXGroupEditor.hpp SFXGroupEditor.cpp
        SongGroupEditor.hpp SongGroupEditor.cpp
        AudioGroupModel.hpp AudioGroupModel.cpp
        resources/resources.qrc qrc_resources.cpp
        ${PLAT_SRCS}
        main.cpp)
if(COMMAND add_sanitizers)
  add_sanitizers(amuse-gui)
endif()

set_target_properties(amuse-gui PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platforms/mac/Info.plist")

target_link_libraries(amuse-gui ${PLAT_LIBS}
        ${Qt5Widgets_LIBRARIES}
        ${Qt5Network_LIBRARIES}
        ${Qt5Xml_LIBRARIES}
        ${Qt5Svg_LIBRARIES}
        amuse boo ${BOO_SYS_LIBS} logvisor zeus athena-core athena-libyaml xxhash ${ZLIB_LIBRARIES} ${LZO_LIB})
