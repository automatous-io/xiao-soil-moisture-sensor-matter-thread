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

#
#   docker build -t automatous-io/xiao-soil-builder:idf-v5.5.2 .devcontainer
#
# Stock ESP-IDF and nothing else. Matter arrives as the espressif/esp_matter
# managed component from the ESP Component Registry, pinned by
# source/dependencies.lock.
#
# espressif/idf:v5.5.2 would also work. It is not used because it carries
# IDF's full submodule history; the shallow clone below produces the same
# toolchain in roughly a third of the disk.
FROM ubuntu:24.04@sha256:561618e2c15bf2397621dd04f96926663a3b5616c189cf7e38db7e82f5c538ea

# Matches the `idf` version recorded in source/dependencies.lock.
ARG IDF_CHECKOUT_REF=v5.5.2
ARG IDF_CLONE_URL=https://github.com/espressif/esp-idf.git
ARG IDF_INSTALL_TARGETS=esp32c6

ENV DEBIAN_FRONTEND=noninteractive
ENV IDF_PATH=/opt/esp/idf
ENV IDF_TOOLS_PATH=/opt/esp
ENV IDF_PYTHON_CHECK_CONSTRAINTS=no
ENV IDF_CCACHE_ENABLE=1

RUN set -x \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build ccache \
        bison flex gperf make pkg-config \
        python3 python3-venv python3-dev python3-pip python-is-python3 \
        git curl wget ca-certificates unzip xz-utils bzip2 zip \
        libffi-dev libssl-dev libncurses-dev libusb-1.0-0-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/* \
    && rm -rf /usr/share/doc/* /usr/share/man/*

# --shallow-submodules is what keeps this small; IDF's submodule history is the
# bulk of the espressif/idf published image.
RUN set -x \
    && git clone --depth=1 --branch=${IDF_CHECKOUT_REF} \
        --recurse-submodules --shallow-submodules \
        ${IDF_CLONE_URL} ${IDF_PATH} \
    && git config --system --add safe.directory ${IDF_PATH}

RUN set -x \
    && ${IDF_PATH}/tools/idf_tools.py --non-interactive install required --targets=${IDF_INSTALL_TARGETS} \
    && ${IDF_PATH}/tools/idf_tools.py --non-interactive install cmake \
    && ${IDF_PATH}/tools/idf_tools.py --non-interactive install-python-env \
    && rm -rf ${IDF_TOOLS_PATH}/dist /root/.cache/pip

RUN cd ${IDF_PATH} && git describe --tags --always > /opt/esp/esp-idf-version.txt

# The workspace is bind-mounted from the host user while this runs as root, and
# IDF shells out to git for the app version.
RUN git config --system --add safe.directory '*'

# VS Code terminals are non-login interactive shells, which read bash.bashrc.
RUN printf '%s\n' \
    'if [ -z "${IDF_EXPORT_DONE:-}" ]; then' \
    '  . "$IDF_PATH/export.sh" >/dev/null 2>&1 || true' \
    '  export IDF_EXPORT_DONE=1' \
    'fi' >> /etc/bash.bashrc

WORKDIR /workspaces
CMD ["/bin/bash"]
