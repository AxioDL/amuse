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
    lib/ContainerRegistry.cpp
    lib/DSPCodec.c
    lib/N64MusyXCodec.c)

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
    include/amuse/ContainerRegistry.hpp
    include/amuse/Common.hpp
    include/amuse/amuse.hpp
    include/amuse/DSPCodec.h
    include/amuse/N64MusyXCodec.h)

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

      # Search for provision profile to make AudioUnit extension on OS X
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

        # Extension App
        add_executable(amuse-au MACOSX_BUNDLE AudioUnit/AudioUnitBackend.hpp AudioUnit/AudioUnitBackend.mm
                       AudioUnit/AudioUnitViewController.hpp AudioUnit/AudioUnitViewController.mm
                       AudioUnit/AudioGroupFilePresenter.hpp AudioUnit/AudioGroupFilePresenter.mm)

        set(APPLE_BUNDLE_ID "io.github.axiodl.Amuse.AudioUnit")
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/AmuseExtension.entitlements.in
                       ${CMAKE_CURRENT_BINARY_DIR}/AmuseExtension.entitlements)

        target_link_libraries(amuse-au amuse boo soxr ${AUDIOUNIT_LIBRARY} ${COREAUDIOKIT_LIBRARY}
                              ${AVFOUNDATION_LIBRARY} ${BOO_SYS_LIBS} logvisor athena-core)
        set_target_properties(amuse-au PROPERTIES
                              MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/ExtensionInfo.plist"
                              BUNDLE_EXTENSION "appex" BUNDLE TRUE
                              XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_BINARY_DIR}/AmuseExtension.entitlements"
                              XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${APPLE_DEV_ID}"
                              LINK_FLAGS "-e _NSExtensionMain -fobjc-arc -fobjc-link-runtime -fapplication-extension")


        # Containing App
        add_custom_command(
          OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/AmuseContainerMainMenu.nib
          COMMAND ibtool --errors --warnings --notices --module amuse_au_container --auto-activate-custom-fonts
          --target-device mac --minimum-deployment-target 10.11 --output-format human-readable-text --compile
          ${CMAKE_CURRENT_BINARY_DIR}/AmuseContainerMainMenu.nib
          ${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/AmuseContainerMainMenu.xib
          DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/AmuseContainerMainMenu.xib
        )
        add_executable(amuse-au-container MACOSX_BUNDLE AudioUnit/AmuseContainingApp.mm
                       AudioUnit/AudioUnitBackend.hpp AudioUnit/AudioUnitBackend.mm
                       AudioUnit/AudioUnitViewController.hpp AudioUnit/AudioUnitViewController.mm
                       AudioUnit/AudioGroupFilePresenter.hpp AudioUnit/AudioGroupFilePresenter.mm
                       AmuseContainerMainMenu.nib)
        set_source_files_properties(AudioUnit/AudioUnitBackend.mm AudioUnit/AudioUnitViewController.mm
                                    AudioUnit/AmuseContainingApp.mm AudioUnit/AudioGroupFilePresenter.mm
                                    PROPERTIES COMPILE_FLAGS -fobjc-arc)
        target_link_libraries(amuse-au-container amuse boo soxr ${AUDIOUNIT_LIBRARY} ${COREAUDIOKIT_LIBRARY}
                              ${AVFOUNDATION_LIBRARY} ${BOO_SYS_LIBS} logvisor athena-core)

        set(APPLE_BUNDLE_ID "io.github.axiodl.Amuse")
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/AmuseContainer.entitlements.in
                       ${CMAKE_CURRENT_BINARY_DIR}/AmuseContainer.entitlements)

        set_target_properties(amuse-au-container PROPERTIES
                              MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/AudioUnit/ContainerInfo.plist"
                              XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_BINARY_DIR}/AmuseContainer.entitlements"
                              XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${APPLE_DEV_ID}")

        add_custom_command(TARGET amuse-au POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy "${PROV_PROFILE}" "$<TARGET_FILE_DIR:amuse-au>/../embedded.provisionprofile"
          COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:amuse-au-container>/../PlugIns/amuse-au.appex"
          COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:amuse-au>/../.."
                      "$<TARGET_FILE_DIR:amuse-au-container>/../PlugIns/amuse-au.appex"
          COMMAND codesign --force --sign
                      ${APPLE_DEV_ID} --entitlements "${CMAKE_CURRENT_BINARY_DIR}/AmuseExtension.entitlements"
                      "$<TARGET_FILE_DIR:amuse-au-container>/../PlugIns/amuse-au.appex"
          COMMAND codesign --force --sign ${APPLE_DEV_ID}
                      "$<TARGET_FILE_DIR:amuse-au-container>/../.."
          VERBATIM
        )

        add_custom_command(TARGET amuse-au-container POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          "${CMAKE_CURRENT_BINARY_DIR}/AmuseContainerMainMenu.nib"
          "$<TARGET_FILE_DIR:amuse-au-container>/../Resources/AmuseContainerMainMenu.nib"
          COMMAND codesign --force --sign
                      ${APPLE_DEV_ID} "$<TARGET_FILE_DIR:amuse-au-container>/../.."
          VERBATIM
        )

      else()
        message(WARNING "Unable to find developer provision profile; skipping Amuse-AU")
      endif()
    endif()
  endif()

  add_executable(amuseplay WIN32 driver/main.cpp)
  target_link_libraries(amuseplay amuse boo ${BOO_SYS_LIBS} logvisor athena-core ${ZLIB_LIBRARIES})
endif()
