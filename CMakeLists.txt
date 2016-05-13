set(SOURCES
    lib/AudioGroup.cpp
    lib/AudioGroupData.cpp
    lib/AudioGroupPool.cpp
    lib/AudioGroupProject.cpp
    lib/AudioGroupSampleDirectory.cpp
    lib/Emitter.cpp
    lib/Engine.cpp
    lib/Envelope.cpp
    lib/Listener.cpp
    lib/Sequencer.cpp
    lib/SoundMacroState.cpp
    lib/Voice.cpp
    lib/Submix.cpp
    lib/EffectBase.cpp
    lib/EffectReverb.cpp
    lib/EffectChorus.cpp
    lib/EffectDelay.cpp
    lib/SurroundProfiles.cpp
    lib/dsp.c)

set(HEADERS
    include/amuse/AudioGroup.hpp
    include/amuse/AudioGroupData.hpp
    include/amuse/AudioGroupPool.hpp
    include/amuse/AudioGroupProject.hpp
    include/amuse/AudioGroupSampleDirectory.hpp
    include/amuse/Emitter.hpp
    include/amuse/Engine.hpp
    include/amuse/Entity.hpp
    include/amuse/Envelope.hpp
    include/amuse/Listener.hpp
    include/amuse/Sequencer.hpp
    include/amuse/SoundMacroState.hpp
    include/amuse/Voice.hpp
    include/amuse/Submix.hpp
    include/amuse/IBackendSubmix.hpp
    include/amuse/IBackendVoice.hpp
    include/amuse/IBackendVoiceAllocator.hpp
    include/amuse/EffectBase.hpp
    include/amuse/EffectReverb.hpp
    include/amuse/EffectChorus.hpp
    include/amuse/EffectDelay.hpp
    include/amuse/SurroundProfiles.hpp
    include/amuse/Common.hpp
    include/amuse/amuse.hpp
    include/amuse/dsp.h)

unset(EXTRAS)
if(TARGET boo)
  include_directories(${BOO_INCLUDE_DIR} ${LOGVISOR_INCLUDE_DIR} ${ATHENA_INCLUDE_DIR})
  list(APPEND EXTRAS lib/BooBackend.cpp include/amuse/BooBackend.hpp)
endif()

include_directories(include)
set(AMUSE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE PATH "amuse include path" FORCE)

add_library(amuse
            ${SOURCES}
            ${HEADERS}
            ${EXTRAS})

if(TARGET boo)
  add_executable(amusetool WIN32 driver/main.cpp)
  target_link_libraries(amusetool amuse boo ${BOO_SYS_LIBS} logvisor athena-core)
endif()
