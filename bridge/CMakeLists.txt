# CrowLink - wireless trainer bridge for CrowMotion.
# Top-level ESP-IDF project file. Target board: ESP32-C6 Super Mini.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

# Share the CrowLink wire protocol with the tracker.
set(EXTRA_COMPONENT_DIRS "../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(crowlink)
