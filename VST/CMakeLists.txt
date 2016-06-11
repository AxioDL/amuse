set(VST3_SDK_ROOT "" CACHE PATH "Path to VST 3.x SDK directory containing 'public.sdk' and 'plugininterfaces'")
if (WIN32 AND (EXISTS ${VST3_SDK_ROOT}))
  message(STATUS "Found VST SDK; building plugin")
  include_directories(${VST3_SDK_ROOT} ${VST3_SDK_ROOT}/public.sdk/source/vst2.x)
  set(VST2_DIR ${VST3_SDK_ROOT}/public.sdk/source/vst2.x)
  add_definitions(${BOO_SYS_DEFINES})
  add_library(amuse-vst SHARED
              VSTBackend.hpp VSTBackend.cpp
              VSTEditor.hpp VSTEditor.cpp
              ${VST2_DIR}/vstplugmain.cpp
              ${VST2_DIR}/audioeffect.cpp
              ${VST2_DIR}/audioeffectx.cpp)
  target_link_libraries(amuse-vst amuse boo soxr ${ZLIB_LIBRARIES} ${BOO_SYS_LIBS}
                                  Msimg32 logvisor athena-core)
  set_target_properties(amuse-vst PROPERTIES LINK_FLAGS "/EXPORT:VSTPluginMain")
endif()
