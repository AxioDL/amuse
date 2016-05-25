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
    lib/Emitter.cpp
    lib/Engine.cpp
    lib/Envelope.cpp
    lib/Listener.cpp
    lib/Sequencer.cpp
    lib/SoundMacroState.cpp
    lib/SongState.cpp
    lib/Voice.cpp
    lib/VolumeLUT.cpp
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
    include/amuse/SongState.hpp
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
  include_directories(${BOO_INCLUDE_DIR} ${BOO_INCLUDE_DIR}/../lib ${BOO_INCLUDE_DIR}/../soxr/src
                      ${LOGVISOR_INCLUDE_DIR} ${ATHENA_INCLUDE_DIR})
  list(APPEND EXTRAS lib/BooBackend.cpp include/amuse/BooBackend.hpp)
endif()

include_directories(include)
set(AMUSE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE PATH "amuse include path" FORCE)

add_library(amuse
            ${SOURCES}
            ${HEADERS}
            ${EXTRAS})

if(TARGET boo)
  # AudioUnit Target
  if (APPLE AND (NOT CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_GREATER 10.10))
    set(APPLE_DEV_ID "" CACHE STRING "Mac Developer ID string 'Mac Developer: John Smith (XXXXXXXXXX)'")
    set(APPLE_TEAM_ID "" CACHE STRING "Team ID string provisioned within Xcode / Apple's portal")
    find_library(AVFOUNDATION_LIBRARY AVFoundation)
    find_library(AUDIOUNIT_LIBRARY AudioUnit)
    find_library(COREAUDIOKIT_LIBRARY CoreAudioKit)
    if (NOT (AUDIOUNIT_LIBRARY STREQUAL AUDIOUNIT_LIBRARY-NOTFOUND))
      set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}")

      # Search for provision profile to make AudioUnit on OS X
      unset(PROV_PROFILE)
      file(GLOB PROV_FILES "$ENV{HOME}/Library/MobileDevice/Provisioning Profiles/*.provisionprofile")
      foreach(FILE ${PROV_FILES})
        file(STRINGS "${FILE}" NAME REGEX ${APPLE_TEAM_ID})
        if(NAME)
          set(PROV_PROFILE "${FILE}")
          break()
        endif()
      endforeach()

      if(EXISTS "${PROV_PROFILE}")
        # Containing App
        add_executable(amuse-au-container MACOSX_BUNDLE AudioUnit/AmuseContainingApp.mm
                       AudioUnit/AudioUnitBackend.hpp AudioUnit/AudioUnitBackend.mm
                       AudioUnit/AudioUnitViewController.hpp AudioUnit/AudioUnitViewController.mm)
        set_source_files_properties(AudioUnit/AudioUnitBackend.mm AudioUnit/AudioUnitViewController.mm
                                    AudioUnit/AmuseContainingApp.mm
                                    PROPERTIES COMPILE_FLAGS -fobjc-arc)
        target_link_libraries(amuse-au-container amuse boo soxr ${AUDIOUNIT_LIBRARY} ${COREAUDIOKIT_LIBRARY}
                              ${AVFOUNDATION_LIBRARY} ${BOO_SYS_LIBS} logvisor)
        set_target_properties(amuse-au-container PROPERTIES
                              MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/ContainerInfo.plist"
                              XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/Amuse.entitlements")

        set(APPLE_BUNDLE_ID "io.github.axiodl.Amuse")
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/Amuse.entitlements.in
                       ${CMAKE_CURRENT_BINARY_DIR}/AmuseContainer.entitlements)

        # Extension App
        add_executable(amuse-au MACOSX_BUNDLE AudioUnit/AudioUnitBackend.hpp AudioUnit/AudioUnitBackend.mm
                       AudioUnit/AudioUnitViewController.hpp AudioUnit/AudioUnitViewController.mm)

        set(APPLE_BUNDLE_ID "io.github.axiodl.Amuse.AudioUnit")
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/Amuse.entitlements.in
                       ${CMAKE_CURRENT_BINARY_DIR}/AmuseExtension.entitlements)

        target_link_libraries(amuse-au amuse boo soxr ${AUDIOUNIT_LIBRARY} ${COREAUDIOKIT_LIBRARY}
                              ${AVFOUNDATION_LIBRARY} ${BOO_SYS_LIBS} logvisor)
        set_target_properties(amuse-au PROPERTIES
                              MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/ExtensionInfo.plist"
                              BUNDLE_EXTENSION "appex" BUNDLE TRUE
                              XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/Amuse.entitlements"
                              LINK_FLAGS "-e _NSExtensionMain -fobjc-arc -fobjc-link-runtime -fapplication-extension")

        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/amuse-au.app/Contents/embedded.provisionprofile" INPUT "${PROV_PROFILE}")
        install(CODE "file(REMOVE_RECURSE \"${CMAKE_CURRENT_BINARY_DIR}/amuse-au-container.app/Contents/PlugIns/amuse-au.appex\")")
        install(CODE "file(COPY \"${CMAKE_CURRENT_BINARY_DIR}/amuse-au.app/\" DESTINATION
            \"${CMAKE_CURRENT_BINARY_DIR}/amuse-au-container.app/Contents/PlugIns/amuse-au.appex\" PATTERN
            \"${CMAKE_CURRENT_BINARY_DIR}/amuse-au.app/*\")")
        install(CODE "message(STATUS \"Codesigning: amuse-au\")\nexecute_process(COMMAND codesign --force --sign
            \"${APPLE_DEV_ID}\" --entitlements \"${CMAKE_CURRENT_BINARY_DIR}/AmuseExtension.entitlements\"
            \"${CMAKE_CURRENT_BINARY_DIR}/amuse-au-container.app/Contents/PlugIns/amuse-au.appex\")")
        install(CODE "message(STATUS \"Codesigning: amuse-au-container\")\nexecute_process(COMMAND codesign --force --sign
            \"${APPLE_DEV_ID}\" --entitlements \"${CMAKE_CURRENT_BINARY_DIR}/AmuseContainer.entitlements\"
            \"${CMAKE_CURRENT_BINARY_DIR}/amuse-au-container.app\")")

      else()
        message(WARNING "Unable to find developer provision profile; skipping Amuse-AU")
      endif()
    endif()
  endif()

  add_executable(amuseplay WIN32 driver/main.cpp)
  target_link_libraries(amuseplay amuse boo ${BOO_SYS_LIBS} logvisor athena-core)
endif()
