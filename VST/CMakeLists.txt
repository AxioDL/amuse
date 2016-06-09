unset(VST_MAIN_SRC CACHE)
find_file(VST_MAIN_SRC vst36/vstplugmain.cpp)
if (NOT (${VST_MAIN_SRC} STREQUAL "VST_MAIN_SRC-NOTFOUND"))
  message(STATUS "${VST_MAIN_SRC} Found VST SDK; building plugin")
  add_library(amuse-vst SHARED
              VSTBackend.hpp VSTBackend.cpp
              VSTEditor.hpp VSTEditor.cpp ${VST_MAIN_SRC})
endif()
