cmake_minimum_required(VERSION 3.10)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets)
find_package(Qt5Network)
find_package(Qt5Xml)
find_package(Qt5Svg)
find_package(Qt5Script)
find_package(Qt5LinguistTools)

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

configure_file(resources/translation_res.qrc translation_res.qrc @ONLY)
set(TRANSLATIONS
    resources/lang_de.ts)
QT5_CREATE_TRANSLATION(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../lib ${TRANSLATIONS})

QT5_ADD_RESOURCES(qrc_resources.cpp resources/resources.qrc)
QT5_ADD_RESOURCES(qrc_translation_res.cpp ${CMAKE_CURRENT_BINARY_DIR}/translation_res.qrc OPTIONS -no-compress)

QT5_WRAP_UI(MAIN_WINDOW_UI MainWindow.ui)

QT5_WRAP_CPP(AMUSE_MOC
        Common.hpp
        MainWindow.hpp
        KeyboardWidget.hpp
        StatusBarWidget.hpp
        ProjectModel.hpp
        ProjectStatistics.hpp
        EditorWidget.hpp
        SoundMacroEditor.hpp
        ADSREditor.hpp
        CurveEditor.hpp
        KeymapEditor.hpp
        LayersEditor.hpp
        SampleEditor.hpp
        SoundGroupEditor.hpp
        SongGroupEditor.hpp)

add_executable(amuse-gui WIN32 MACOSX_BUNDLE
        Common.hpp Common.cpp
        MainWindow.ui ${MAIN_WINDOW_UI} MainWindow.hpp MainWindow.cpp
        KeyboardWidget.hpp KeyboardWidget.cpp
        StatusBarWidget.hpp
        ProjectModel.hpp ProjectModel.cpp
        ProjectStatistics.hpp ProjectStatistics.cpp
        EditorWidget.hpp EditorWidget.cpp
        SoundMacroEditor.hpp SoundMacroEditor.cpp
        ADSREditor.hpp ADSREditor.cpp
        CurveEditor.hpp CurveEditor.cpp
        KeymapEditor.hpp KeymapEditor.cpp
        LayersEditor.hpp LayersEditor.cpp
        SampleEditor.hpp SampleEditor.cpp
        SoundGroupEditor.hpp SoundGroupEditor.cpp
        SongGroupEditor.hpp SongGroupEditor.cpp
        MIDIReader.hpp MIDIReader.cpp
        resources/resources.qrc qrc_resources.cpp
        ${QM_FILES} qrc_translation_res.cpp
        ${AMUSE_MOC} ${PLAT_SRCS}
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
        ${Qt5Script_LIBRARIES}
        amuse boo ${BOO_SYS_LIBS} logvisor athena-core athena-libyaml xxhash ${ZLIB_LIBRARIES} ${LZO_LIB})
