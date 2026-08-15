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
Build the Matter OTA image (.ota) from the firmware build output.

    python3 scripts/make-matter-ota.py           # project dir defaults to source/
    python3 scripts/make-matter-ota.py source

Every field that the device matches against is read from the build:

  - vendor / product ID from source/sdkconfig (CONFIG_DEVICE_VENDOR_ID /
    CONFIG_DEVICE_PRODUCT_ID).
  - software version (number + string) from main/CHIPProjectConfig.h
    (CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION[_STRING]), cross-checked
    against PROJECT_VER / PROJECT_VER_NUMBER in source/CMakeLists.txt. The
    three version locations must agree, and the script refuses to package a
    build that predates a version bump (version string not compiled into the
    .bin). The .ota header cannot claim a version its payload does not carry.

It wraps build/soil-sensor.bin with the upstream Matter ota_image_tool.py and
writes automatous-io-xiao-soil-moisture-sensor-v<version>.ota next to the
build.

Run it in the same environment you build in (the dev container works).
ota_image_tool.py ships inside the esp_matter managed component, fetched by
the first build; set $OTA_IMAGE_TOOL to point elsewhere. A copy under
scripts/ will NOT run on its own: ota_image_tool.py imports matter.tlv from a
sibling directory that only exists inside the connectedhomeip tree, so it has
to run from there.
"""
import json, os, re, subprocess, sys


def sdkconfig_int(sdkconfig, key):
    m = re.search(rf"^{re.escape(key)}=(.+)$", open(sdkconfig).read(), re.M)
    if not m:
        sys.exit(f"{key} not set in {sdkconfig} — run `idf.py build` first")
    return int(m.group(1).strip(), 0)


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


def find_ota_image_tool(project_dir):
    override = os.environ.get("OTA_IMAGE_TOOL")
    if override and os.path.isfile(override):
        return override
    cand = os.path.join(project_dir, "managed_components", "espressif__esp_matter",
                        "connectedhomeip", "connectedhomeip", "src", "app",
                        "ota_image_tool.py")
    if os.path.isfile(cand):
        return cand
    sys.exit("could not find ota_image_tool.py in the esp_matter managed component — "
             "run `idf.py build` first so components resolve, or set $OTA_IMAGE_TOOL")


def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    project_dir = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(repo_root, "source")
    build = os.path.join(project_dir, "build")
    desc = os.path.join(build, "project_description.json")
    if not os.path.isfile(desc):
        sys.exit(f"no build found at {build} — run `idf.py build` first")
    project = json.load(open(desc))["project_name"]

    sdkconfig = os.path.join(project_dir, "sdkconfig")
    header = os.path.join(project_dir, "main", "CHIPProjectConfig.h")
    cmakelists = os.path.join(project_dir, "CMakeLists.txt")
    app_bin = os.path.join(build, f"{project}.bin")
    for p in (sdkconfig, header, cmakelists, app_bin):
        if not os.path.isfile(p):
            sys.exit(f"missing build output: {p}")

    vid = sdkconfig_int(sdkconfig, "CONFIG_DEVICE_VENDOR_ID")
    pid = sdkconfig_int(sdkconfig, "CONFIG_DEVICE_PRODUCT_ID")
    version = int(define(header, "CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION").split()[0], 0)
    version_str = define(header, "CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING").split('"')[1]

    # The version must agree across its three homes (CLAUDE.md: version sync).
    project_ver = cmake_value(cmakelists, "PROJECT_VER")
    project_ver_number = int(cmake_value(cmakelists, "PROJECT_VER_NUMBER"), 0)
    if project_ver != version_str or project_ver_number != version:
        sys.exit(f"version mismatch: CHIPProjectConfig.h says {version_str} ({version}), "
                 f"CMakeLists.txt says {project_ver} ({project_ver_number}) — sync them first")

    # The version above is read from source, but the payload comes from
    # build/. A build that predates a version bump would produce an .ota
    # whose header claims a version its payload does not carry. The device
    # flashes it, reboots, reports the old version, and the controller
    # marks the update failed.
    if version_str.encode() not in open(app_bin, "rb").read():
        sys.exit(f"version {version_str} not found in {app_bin} — rebuild before packaging")

    out = os.path.join(project_dir, f"automatous-io-xiao-soil-moisture-sensor-v{version_str}.ota")

    cmd = [sys.executable, find_ota_image_tool(project_dir), "create",
           "-v", f"0x{vid:04X}", "-p", f"0x{pid:04X}",
           "-vn", str(version), "-vs", version_str,
           "-da", "sha256", app_bin, out]
    subprocess.run(cmd, check=True)

    print(f"Created {os.path.basename(out)} ({os.path.getsize(out) / 1024 / 1024:.1f} MB)")
    print(f"  {project}  VID 0x{vid:04X}  PID 0x{pid:04X}  version {version} ({version_str})")


if __name__ == "__main__":
    main()
