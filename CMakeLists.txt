project(amuse)

if(EXISTS boo)
  add_subdirectory(boo)
endif()

set(SOURCES
    lib/AudioGroup.cpp
    lib/AudioGroupData.cpp
    lib/AudioGroupPool.cpp
    lib/AudioGroupProject.cpp
    lib/AudioGroupSampleDirectory.cpp
    lib/DirectoryEnumerator.cpp
    lib/Emitter.cpp
    lib/Engine.cpp
    lib/Envelope.cpp
    lib/Listener.cpp
    lib/Sequencer.cpp
    lib/SoundMacroState.cpp
    lib/SongConverter.cpp
    lib/SongState.cpp
    lib/Voice.cpp
    lib/VolumeLUT.cpp
    lib/Submix.cpp
    lib/Studio.cpp
    lib/EffectBase.cpp
    lib/EffectReverb.cpp
    lib/EffectChorus.cpp
    lib/EffectDelay.cpp
    lib/SurroundProfiles.cpp
    lib/ContainerRegistry.cpp
    lib/DSPCodec.c
    lib/N64MusyXCodec.c)

set(HEADERS
    include/amuse/AudioGroup.hpp
    include/amuse/AudioGroupData.hpp
    include/amuse/AudioGroupPool.hpp
    include/amuse/AudioGroupProject.hpp
    include/amuse/AudioGroupSampleDirectory.hpp
    include/amuse/DirectoryEnumerator.hpp
    include/amuse/Emitter.hpp
    include/amuse/Engine.hpp
    include/amuse/Entity.hpp
    include/amuse/Envelope.hpp
    include/amuse/Listener.hpp
    include/amuse/Sequencer.hpp
    include/amuse/SoundMacroState.hpp
    include/amuse/SongConverter.hpp
    include/amuse/SongState.hpp
    include/amuse/Voice.hpp
    include/amuse/Submix.hpp
    include/amuse/Studio.hpp
    include/amuse/IBackendSubmix.hpp
    include/amuse/IBackendVoice.hpp
    include/amuse/IBackendVoiceAllocator.hpp
    include/amuse/EffectBase.hpp
    include/amuse/EffectReverb.hpp
    include/amuse/EffectChorus.hpp
    include/amuse/EffectDelay.hpp
    include/amuse/SurroundProfiles.hpp
    include/amuse/ContainerRegistry.hpp
    include/amuse/Common.hpp
    include/amuse/amuse.hpp
    include/amuse/DSPCodec.h
    include/amuse/N64MusyXCodec.h)

unset(EXTRAS)
if(TARGET boo)
  include_directories(${BOO_INCLUDE_DIR} ${BOO_INCLUDE_DIR}/../lib ${BOO_INCLUDE_DIR}/../soxr/src
                      ${LOGVISOR_INCLUDE_DIR} ${ATHENA_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})
  list(APPEND EXTRAS lib/BooBackend.cpp include/amuse/BooBackend.hpp)
endif()

include_directories(include)
set(AMUSE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE PATH "amuse include path" FORCE)

add_library(amuse
            ${SOURCES}
            ${HEADERS}
            ${EXTRAS})

if(TARGET boo)
  # AudioUnit Target (OS X only)
  add_subdirectory(AudioUnit)

  # VST Target
  add_subdirectory(VST)

  # Multi-platform CLI tools

  # Player
  add_executable(amuseplay WIN32 driver/amuseplay.cpp)
  target_link_libraries(amuseplay amuse boo ${BOO_SYS_LIBS} logvisor athena-core ${ZLIB_LIBRARIES})

  # Converter
  add_executable(amuseconv driver/amuseconv.cpp)
  target_link_libraries(amuseconv amuse ${BOO_SYS_LIBS} logvisor athena-core ${ZLIB_LIBRARIES})

  # Renderer
  add_executable(amuserender driver/amuserender.cpp)
  target_link_libraries(amuserender amuse boo ${BOO_SYS_LIBS} logvisor athena-core ${ZLIB_LIBRARIES})
endif()
