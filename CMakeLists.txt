# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 Daniel Campos Ramos <Capitain_Jack@yahoo.com>
cmake_minimum_required(VERSION 3.25)
project(StereoCreate VERSION 1.0.0 LANGUAGES CXX)
find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(CTest)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons I18n JobWidgets KIO)
find_package(StereoLayer 1.4 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil)

ecm_setup_version(PROJECT VARIABLE_PREFIX STEREO_CREATE VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/stereo_create_version.h)
add_definitions(-DTRANSLATION_DOMAIN=\"stereo-create\")

add_subdirectory(src)
ki18n_install(po)
# The application the job notifications name, not shown in menus
install(FILES data/org.sparky.stereo-create.desktop DESTINATION ${KDE_INSTALL_APPDIR})
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test DBus)
    add_subdirectory(autotests)
endif()
