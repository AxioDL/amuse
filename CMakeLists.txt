cmake_minimum_required(VERSION 3.10 FATAL_ERROR) # because of c++17

project(amuse)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
    add_compile_options(-Wno-narrowing)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/boo AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/athena)
  message(STATUS "Preparing standalone build")
  add_subdirectory(boo)
  add_subdirectory(athena)
endif()

add_library(amuse
  lib/AudioGroup.cpp
  lib/AudioGroupData.cpp
  lib/AudioGroupPool.cpp
  lib/AudioGroupProject.cpp
  lib/AudioGroupSampleDirectory.cpp
  lib/Common.cpp
  lib/ContainerRegistry.cpp
  lib/DirectoryEnumerator.cpp
  lib/DSPCodec.cpp
  lib/EffectChorus.cpp
  lib/EffectDelay.cpp
  lib/EffectReverb.cpp
  lib/Emitter.cpp
  lib/Engine.cpp
  lib/Envelope.cpp
  lib/Listener.cpp
  lib/N64MusyXCodec.cpp
  lib/Sequencer.cpp
  lib/SongConverter.cpp
  lib/SongState.cpp
  lib/SoundMacroState.cpp
  lib/Studio.cpp
  lib/Submix.cpp
  lib/Voice.cpp
  lib/VolumeTable.cpp

  include/amuse/amuse.hpp
  include/amuse/AudioGroup.hpp
  include/amuse/AudioGroupData.hpp
  include/amuse/AudioGroupPool.hpp
  include/amuse/AudioGroupProject.hpp
  include/amuse/AudioGroupSampleDirectory.hpp
  include/amuse/Common.hpp
  include/amuse/ContainerRegistry.hpp
  include/amuse/DirectoryEnumerator.hpp
  include/amuse/DSPCodec.hpp
  include/amuse/EffectBase.hpp
  include/amuse/EffectChorus.hpp
  include/amuse/EffectDelay.hpp
  include/amuse/EffectReverb.hpp
  include/amuse/Emitter.hpp
  include/amuse/Engine.hpp
  include/amuse/Entity.hpp
  include/amuse/Envelope.hpp
  include/amuse/IBackendSubmix.hpp
  include/amuse/IBackendVoice.hpp
  include/amuse/IBackendVoiceAllocator.hpp
  include/amuse/Listener.hpp
  include/amuse/N64MusyXCodec.hpp
  include/amuse/Sequencer.hpp
  include/amuse/SongConverter.hpp
  include/amuse/SoundMacroState.hpp
  include/amuse/SongState.hpp
  include/amuse/Submix.hpp
  include/amuse/Studio.hpp
  include/amuse/Voice.hpp
  include/amuse/VolumeTable.hpp
)

if(NX)
  target_sources(amuse PRIVATE include/switch_math.hpp)
endif()

target_atdna(amuse atdna_AudioGroupPool.cpp include/amuse/AudioGroupPool.hpp)
target_atdna(amuse atdna_AudioGroupProject.cpp include/amuse/AudioGroupProject.hpp)
target_atdna(amuse atdna_AudioGroupSampleDirectory.cpp include/amuse/AudioGroupSampleDirectory.hpp)
target_include_directories(amuse PUBLIC include)
target_link_libraries(amuse athena-core ${ZLIB_LIBRARIES} lzokay)
if(TARGET boo)
  target_sources(amuse PRIVATE lib/BooBackend.cpp include/amuse/BooBackend.hpp)
  target_link_libraries(amuse boo)
endif()
if (NOT MSVC)
  target_compile_options(amuse PRIVATE -Wno-unknown-pragmas)
endif()
if(COMMAND add_sanitizers)
  add_sanitizers(amuse)
endif()

if(TARGET boo AND NOT WINDOWS_STORE AND NOT NX)
  # AudioUnit Target (OS X only)
  add_subdirectory(AudioUnit)

  # VST Target
  add_subdirectory(VST)

  # Multi-platform CLI tools

  # Player
  add_executable(amuseplay WIN32 driver/amuseplay.cpp)
  target_link_libraries(amuseplay amuse logvisor)

  # Converter
  add_executable(amuseconv driver/amuseconv.cpp)
  target_link_libraries(amuseconv amuse logvisor)

  # Renderer
  add_executable(amuserender driver/amuserender.cpp)
  target_link_libraries(amuserender amuse logvisor)

  if(COMMAND add_sanitizers)
    add_sanitizers(amuseplay)
    add_sanitizers(amuseconv)
    add_sanitizers(amuserender)
  endif()

  # Editor
  find_package(Qt5 COMPONENTS Widgets)
  if (Qt5Widgets_FOUND)
    message(STATUS "Qt5 found, amuse-gui will be built")
    add_subdirectory(Editor)
  else()
    message(STATUS "Qt5 not found, amuse-gui will not be built")
  endif()
endif()
