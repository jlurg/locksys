# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# C unit-test image: Ruby 3.4, Ceedling 1.1.9 (Unity, CMock), GCC and gcovr 8.6.
# Tag: CEEDLING_IMAGE in tools/versions.env. Build and run through tools/docker/ceedling/run.sh.
# The base image is pinned by digest; Debian packages come from the trixie release of that
# image (stable security updates only).

FROM ruby:4.0.7-slim-trixie@sha256:db9ddd17cc6ac603f2497d98ac5c88e4118908d6f9a45f2422ebee141f91e485

ARG CEEDLING_VERSION=1.1.9
ARG GCOVR_VERSION=8.6

RUN apt-get update \
    && apt-get install --yes --no-install-recommends gcc libc6-dev make python3 python3-venv \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/gcovr \
    && /opt/gcovr/bin/pip install --no-cache-dir --disable-pip-version-check "gcovr==${GCOVR_VERSION}"

RUN gem install --no-document ceedling --version "${CEEDLING_VERSION}"

ENV PATH="/opt/gcovr/bin:${PATH}" \
    HOME=/tmp

RUN ceedling version && gcovr --version && gcc --version

WORKDIR /work
CMD ["ceedling", "version"]
