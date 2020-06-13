add_executable(amuse-mkqticon mkqticon.c)
target_link_libraries(amuse-mkqticon ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})
target_include_directories(amuse-mkqticon PRIVATE ${PNG_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})

macro(declare_qticon_target)
add_custom_command(OUTPUT ${amuse_BINARY_DIR}/Editor/platforms/freedesktop/mainicon_qt.bin
                   COMMAND $<TARGET_FILE:amuse-mkqticon>
                   ARGS ${amuse_BINARY_DIR}/Editor/platforms/freedesktop/mainicon_qt.bin
                   DEPENDS
                   ${amuse_SOURCE_DIR}/Editor/platforms/freedesktop/128x128/apps/amuse-gui.png
                   ${amuse_SOURCE_DIR}/Editor/platforms/freedesktop/64x64/apps/amuse-gui.png
                   ${amuse_SOURCE_DIR}/Editor/platforms/freedesktop/48x48/apps/amuse-gui.png
                   ${amuse_SOURCE_DIR}/Editor/platforms/freedesktop/32x32/apps/amuse-gui.png
                   ${amuse_SOURCE_DIR}/Editor/platforms/freedesktop/16x16/apps/amuse-gui.png
                   WORKING_DIRECTORY ${amuse_SOURCE_DIR}/Editor/platforms/freedesktop
                   COMMENT "Generating mainicon_qt.bin")
bintoc(mainicon_qt.cpp ${amuse_BINARY_DIR}/Editor/platforms/freedesktop/mainicon_qt.bin MAINICON_QT)
endmacro()
