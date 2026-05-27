# SPDX-License-Identifier: MIT
"""PlatformIO pre-build: remove stray/corrupt managed_component paths before CMake."""

import os
import subprocess

Import("env")

PROJECT_DIR = env.subst("$PROJECT_DIR")
MANAGED = os.path.join(PROJECT_DIR, "managed_components")

# Local vendored lvgl lives in components/lvgl — do not delete it here.
STRAY_PATHS = (
    os.path.join(MANAGED, "components"),
    os.path.join(MANAGED, "lvgl__lvgl"),
)

for path in STRAY_PATHS:
    if os.path.isdir(path):
        print("[pre] Removing stray path: %s" % path)
        subprocess.run(["rm", "-rf", path], check=True)

CHECKS = (
    ("espressif__cmake_utilities", "package_manager.cmake"),
    ("espressif__esp_lcd_st7796", "CMakeLists.txt"),
    ("espressif__esp_lvgl_port", "CMakeLists.txt"),
)


def _remove_if_corrupt(name, marker_rel):
    comp_dir = os.path.join(MANAGED, name)
    marker = os.path.join(comp_dir, marker_rel)
    if os.path.isdir(comp_dir) and not os.path.isfile(marker):
        print("[pre] Removing incomplete managed component: %s" % name)
        subprocess.run(["rm", "-rf", comp_dir], check=True)


if os.path.isdir(MANAGED):
    for name, marker in CHECKS:
        _remove_if_corrupt(name, marker)
