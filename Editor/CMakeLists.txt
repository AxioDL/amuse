cmake_minimum_required(VERSION 3.10)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Automatically handle invoking moc
set(CMAKE_AUTOMOC ON)

find_package(Qt5 COMPONENTS LinguistTools Network Qml Svg Widgets Xml REQUIRED)

configure_file(resources/translation_res.qrc translation_res.qrc @ONLY)
set(TRANSLATIONS
  resources/lang_de.ts
)
QT5_CREATE_TRANSLATION(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../lib ${TRANSLATIONS})

QT5_ADD_RESOURCES(qrc_resources.cpp resources/resources.qrc)
QT5_ADD_RESOURCES(qrc_translation_res.cpp ${CMAKE_CURRENT_BINARY_DIR}/translation_res.qrc OPTIONS -no-compress)

QT5_WRAP_UI(MAIN_WINDOW_UI MainWindow.ui)

add_executable(amuse-gui WIN32 MACOSX_BUNDLE
  ADSREditor.cpp
  ADSREditor.hpp
  Common.cpp
  Common.hpp
  CurveEditor.cpp
  CurveEditor.hpp
  EditorWidget.cpp
  EditorWidget.hpp
  KeyboardWidget.cpp
  KeyboardWidget.hpp
  KeymapEditor.cpp
  KeymapEditor.hpp
  LayersEditor.cpp
  LayersEditor.hpp
  MainWindow.cpp
  MainWindow.hpp
  MainWindow.ui
  MIDIReader.cpp
  MIDIReader.hpp
  NewSoundMacroDialog.cpp
  NewSoundMacroDialog.hpp
  ProjectModel.cpp
  ProjectModel.hpp
  SampleEditor.cpp
  SampleEditor.hpp
  SongGroupEditor.cpp
  SongGroupEditor.hpp
  SoundGroupEditor.cpp
  SoundGroupEditor.hpp
  SoundMacroEditor.cpp
  SoundMacroEditor.hpp
  StatusBarWidget.cpp
  StatusBarWidget.hpp
  StudioSetupWidget.cpp
  StudioSetupWidget.hpp

  main.cpp
  qrc_resources.cpp
  qrc_translation_res.cpp
  resources/resources.qrc

  ${MAIN_WINDOW_UI}
  ${QM_FILES}
)

if(WIN32)
  target_sources(amuse-gui PRIVATE
    platforms/win/amuse-gui.rc
    platforms/win/amuse-gui.manifest
  )
elseif(APPLE)
  target_sources(amuse-gui PRIVATE
    MacOSExtras.mm
    platforms/mac/mainicon.icns
  )
  set_source_files_properties(platforms/mac/mainicon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
  )
endif()

add_subdirectory(platforms/freedesktop)
declare_qticon_target()
target_sources(amuse-gui PRIVATE mainicon_qt.cpp)

if(COMMAND add_sanitizers)
  add_sanitizers(amuse-gui)
endif()

set_target_properties(amuse-gui PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platforms/mac/Info.plist"
)

target_link_libraries(amuse-gui
  ${PLAT_LIBS}

  Qt5::Network
  Qt5::Qml
  Qt5::Svg
  Qt5::Widgets
  Qt5::Xml

  amuse
  athena-core
  boo
  ${BOO_SYS_LIBS}
  logvisor
  lzokay
  xxhash
  ${ZLIB_LIBRARIES}
)
