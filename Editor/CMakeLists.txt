cmake_minimum_required(VERSION 3.10)

# Automatically handle invoking autorcc, moc, and uic.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 COMPONENTS LinguistTools Network Qml Svg Widgets Xml REQUIRED)

configure_file(resources/translation_res.qrc translation_res.qrc @ONLY)
set(TRANSLATIONS
  resources/lang_de.ts
)
QT5_CREATE_TRANSLATION(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../lib ${TRANSLATIONS})

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

  resources/resources.qrc
  ${CMAKE_CURRENT_BINARY_DIR}/translation_res.qrc

  ${QM_FILES}
)

target_compile_definitions(amuse-gui PRIVATE
  # Disable implicit conversions from ASCII to QString.
  -DQT_NO_CAST_FROM_ASCII
  -DQT_NO_CAST_TO_ASCII

  # Disable narrowing conversions in signal/slot connect() calls.
  -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT

  # Disable unsafe overloads of QProcess' start() function.
  -DQT_NO_PROCESS_COMBINED_ARGUMENT_START

  # Disable implicit QString->QUrl conversions to enforce use of proper resolving functions.
  -DQT_NO_URL_CAST_FROM_STRING

  # Allows for more efficient string concatenation, resulting in less temporaries.
  -DQT_USE_QSTRINGBUILDER 
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
