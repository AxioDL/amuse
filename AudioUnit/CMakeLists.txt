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
      add_executable(amuse-au MACOSX_BUNDLE AudioUnitBackend.hpp AudioUnitBackend.mm
                     AudioUnitViewController.hpp AudioUnitViewController.mm
                     AudioGroupFilePresenter.hpp AudioGroupFilePresenter.mm)

      set(APPLE_BUNDLE_ID "io.github.axiodl.Amuse.AudioUnit")
      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/AmuseExtension.entitlements.in
                     ${CMAKE_CURRENT_BINARY_DIR}/AmuseExtension.entitlements)

      target_link_libraries(amuse-au amuse boo soxr ${AUDIOUNIT_LIBRARY} ${COREAUDIOKIT_LIBRARY}
                            ${AVFOUNDATION_LIBRARY} ${BOO_SYS_LIBS} logvisor athena-core)
      set_target_properties(amuse-au PROPERTIES
                            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ExtensionInfo.plist"
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
        ${CMAKE_CURRENT_SOURCE_DIR}/AmuseContainerMainMenu.xib
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/AmuseContainerMainMenu.xib
      )
      add_executable(amuse-au-container MACOSX_BUNDLE AmuseContainingApp.mm
                     AudioUnitBackend.hpp AudioUnitBackend.mm
                     AudioUnitViewController.hpp AudioUnitViewController.mm
                     AudioGroupFilePresenter.hpp AudioGroupFilePresenter.mm
                     AmuseContainerMainMenu.nib)
      set_source_files_properties(AudioUnitBackend.mm AudioUnitViewController.mm
                                  AmuseContainingApp.mm AudioGroupFilePresenter.mm
                                  PROPERTIES COMPILE_FLAGS -fobjc-arc)
      target_link_libraries(amuse-au-container amuse boo soxr ${AUDIOUNIT_LIBRARY} ${COREAUDIOKIT_LIBRARY}
                            ${AVFOUNDATION_LIBRARY} ${ZLIB_LIBRARIES} ${BOO_SYS_LIBS} logvisor athena-core)

      set(APPLE_BUNDLE_ID "io.github.axiodl.Amuse")
      configure_file(${CMAKE_CURRENT_SOURCE_DIR}/AmuseContainer.entitlements.in
                     ${CMAKE_CURRENT_BINARY_DIR}/AmuseContainer.entitlements)

      set_target_properties(amuse-au-container PROPERTIES
                            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ContainerInfo.plist"
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
