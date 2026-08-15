#!/usr/bin/env python3
#
# Copyright 2026 AUTOMATOUS.IO
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""
Build the merged release image (.bin, flashable at offset 0x0) from the
firmware build output.

    python3 scripts/make-release-bin.py           # project dir defaults to source/
    python3 scripts/make-release-bin.py source

The software version is read from main/CHIPProjectConfig.h, cross-checked
against PROJECT_VER / PROJECT_VER_NUMBER in source/CMakeLists.txt, and
required to be compiled into the built app binary, the same guards as
scripts/make-matter-ota.py. A build that predates a version bump is refused
rather than packaged under the wrong name.

The merge itself drives esptool with the build's own flash_args, producing
automatous-io-xiao-soil-moisture-sensor-v<version>.bin next to the build.
Flash it with:

    esptool.py --chip esp32c6 write_flash 0x0 automatous-io-xiao-soil-moisture-sensor-vX.Y.Z.bin

Run it in the same environment you build in (the dev container works).
"""
import json, os, re, subprocess, sys


def define(header, key):
    m = re.search(rf"^#define\s+{re.escape(key)}\s+(.+)$", open(header).read(), re.M)
    if not m:
        sys.exit(f"{key} not found in {header}")
    return m.group(1).strip()


def cmake_value(cmakelists, key):
    m = re.search(rf"set\({re.escape(key)}\s+\"?([^\")]+)\"?\)", open(cmakelists).read())
    if not m:
        sys.exit(f"{key} not found in {cmakelists}")
    return m.group(1).strip()


def sdkconfig_str(sdkconfig, key):
    m = re.search(rf"^{re.escape(key)}=\"?([^\"\n]+)\"?$", open(sdkconfig).read(), re.M)
    if not m:
        sys.exit(f"{key} not set in {sdkconfig} — run `idf.py build` first")
    return m.group(1).strip()


def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    project_dir = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(repo_root, "source")
    build = os.path.join(project_dir, "build")
    desc = os.path.join(build, "project_description.json")
    if not os.path.isfile(desc):
        sys.exit(f"no build found at {build} — run `idf.py build` first")
    project = json.load(open(desc))["project_name"]

    header = os.path.join(project_dir, "main", "CHIPProjectConfig.h")
    cmakelists = os.path.join(project_dir, "CMakeLists.txt")
    sdkconfig = os.path.join(project_dir, "sdkconfig")
    flash_args = os.path.join(build, "flash_args")
    app_bin = os.path.join(build, f"{project}.bin")
    for p in (header, cmakelists, sdkconfig, flash_args, app_bin):
        if not os.path.isfile(p):
            sys.exit(f"missing build output: {p}")

    version = int(define(header, "CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION").split()[0], 0)
    version_str = define(header, "CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING").split('"')[1]

    # The version must agree across its three homes (CLAUDE.md: version sync).
    project_ver = cmake_value(cmakelists, "PROJECT_VER")
    project_ver_number = int(cmake_value(cmakelists, "PROJECT_VER_NUMBER"), 0)
    if project_ver != version_str or project_ver_number != version:
        sys.exit(f"version mismatch: CHIPProjectConfig.h says {version_str} ({version}), "
                 f"CMakeLists.txt says {project_ver} ({project_ver_number}) — sync them first")

    # The name comes from source, but the payload comes from build/. Refuse
    # a build that predates the version bump instead of shipping it under
    # the new version's name.
    if version_str.encode() not in open(app_bin, "rb").read():
        sys.exit(f"version {version_str} not found in {app_bin} — rebuild before packaging")

    chip = sdkconfig_str(sdkconfig, "CONFIG_IDF_TARGET")
    out = os.path.join(project_dir, f"automatous-io-xiao-soil-moisture-sensor-v{version_str}.bin")

    # flash_args holds offsets and relative paths, meaning esptool must run
    # from the build directory.
    cmd = [sys.executable, "-m", "esptool", "--chip", chip,
           "merge_bin", "-o", out, f"@{flash_args}"]
    subprocess.run(cmd, check=True, cwd=build)

    print(f"Created {os.path.basename(out)} ({os.path.getsize(out) / 1024 / 1024:.1f} MB)")
    print(f"  {project}  {chip}  version {version} ({version_str})  flash at 0x0")


if __name__ == "__main__":
    main()
