# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>

# The engine: every conversion, for the command line and for the context menu
add_library(stereocreate_engine STATIC
    engine/anaglyph.cpp engine/anaglyph.h
    engine/photo.cpp engine/photo.h
    engine/probe.cpp engine/probe.h
    engine/targets.cpp engine/targets.h
    engine/convertvideo.cpp engine/convertvideo.h
    engine/createjob.cpp engine/createjob.h)
set_target_properties(stereocreate_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(stereocreate_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine ${CMAKE_BINARY_DIR})
target_link_libraries(stereocreate_engine PUBLIC Qt6::Gui Qt6::Concurrent KF6::CoreAddons KF6::I18n KF6::KIOCore Stereo::Layer PRIVATE PkgConfig::LIBAV)

add_executable(stereo-create cli/main.cpp)
target_link_libraries(stereo-create stereocreate_engine KF6::JobWidgets Qt6::Widgets)
install(TARGETS stereo-create ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# "Create stereo" in the context menu of stereo pictures and videos (Dolphin and every program with KFileItemActions)
kcoreaddons_add_plugin(stereocreateitemaction SOURCES plugin/stereocreateitemaction.cpp plugin/stereocreateitemaction.h
    INSTALL_NAMESPACE "kf6/kfileitemaction")
target_link_libraries(stereocreateitemaction stereocreate_engine KF6::KIOWidgets Qt6::Widgets)
