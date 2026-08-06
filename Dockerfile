# albdf dev container
#
# Reproducible headless build/test environment for the albdf C++ project
# (fork of PDF4QT: Pdf4QtLibCore + albdf + UnitTests). NO GUI — everything
# runs with QT_QPA_PLATFORM=offscreen.
#
# Build:   docker build -t albdf-dev .
# Run:     docker run -it --rm -v $(pwd):/workspace/albdf \
#                     -w /workspace/albdf albdf-dev
# Inside:  cd src && cmake ... && cmake --build build && bash ../ci/run-ci.sh
#
# The repo is NOT baked in — mount it at /workspace/albdf so edits on the
# host are live inside the container.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV QT_QPA_PLATFORM=offscreen
ENV VCPKG_ROOT=/workspace/vcpkg
# Aliyun mirrors for faster dependency downloads: apt + PyPI.
# NOTE: Aliyun does NOT mirror vcpkg's GitHub repo — that clone stays upstream.
ENV PIP_INDEX_URL=https://mirrors.aliyun.com/pypi/simple/
ENV PIP_TRUSTED_HOST=mirrors.aliyun.com

# ---------------------------------------------------------------------------
# 0. Switch apt to Aliyun Ubuntu mirror BEFORE any apt-get (this is the big
#    download: Qt 6 dev packages are hundreds of MB).
# ---------------------------------------------------------------------------
RUN printf '%s\n' \
    'Types: deb' \
    'URIs: https://mirrors.aliyun.com/ubuntu/' \
    'Suites: noble noble-updates noble-backports' \
    'Components: main restricted universe multiverse' \
    'Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg' \
    '' \
    'Types: deb' \
    'URIs: https://mirrors.aliyun.com/ubuntu/' \
    'Suites: noble-security' \
    'Components: main restricted universe multiverse' \
    'Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg' \
    > /etc/apt/sources.list.d/ubuntu.sources \
    && rm -f /etc/apt/sources.list

# ---------------------------------------------------------------------------
# 1. System packages: build tools + Qt 6 (headless) + RTL/harfbuzz system dev
#    (build deps come from vcpkg in manifest mode; system dev libs are only a
#    convenience for tooling that isn't in vcpkg).
# ---------------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build pkg-config git ca-certificates curl unzip \
        g++ clang clang-format \
        qt6-base-dev qt6-svg-dev qt6-tools-dev qt6-translations-l10n \
        libfontconfig1-dev \
        fonts-liberation \
        python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# 2. vcpkg (deps installed at image build; same tree the repo expects).
#    The project's src/vcpkg.json declares the manifest deps. We install a
#    minimal bootstrap here; the actual per-project deps get installed on the
#    first cmake configure (toolchain auto-installs from the manifest).
#    No Aliyun mirror exists for vcpkg — shallow clone keeps it small.
# ---------------------------------------------------------------------------
RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git /workspace/vcpkg \
    && /workspace/vcpkg/bootstrap-vcpkg.sh -disableMetrics \
    && rm -rf /workspace/vcpkg/buildtrees /workspace/vcpkg/downloads

# ---------------------------------------------------------------------------
# 3. Python tools used by tests/scripts (Aliyun PyPI via PIP_INDEX_URL).
# ---------------------------------------------------------------------------
RUN python3 -m pip install --no-cache-dir --index-url https://mirrors.aliyun.com/pypi/simple/ pillow fonttools

# ---------------------------------------------------------------------------
# 4. Convenience: ccache to speed rebuilds; a non-root dev user is optional —
#    default to root for agent workflows (CI scripts assume root).
# ---------------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends ccache \
    && rm -rf /var/lib/apt/lists/* \
    && echo 'export QT_QPA_PLATFORM=offscreen' >> /root/.bashrc \
    && echo 'export VCPKG_ROOT=/workspace/vcpkg' >> /root/.bashrc \
    && echo 'export PIP_INDEX_URL=https://mirrors.aliyun.com/pypi/simple/' >> /root/.bashrc

WORKDIR /workspace/albdf
CMD ["bash"]
