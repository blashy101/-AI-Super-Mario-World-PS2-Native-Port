# Official PS2DEV v2.0.0 multi-architecture image, pinned by manifest digest.
FROM ghcr.io/ps2dev/ps2dev@sha256:8e6dc4564598ecce94d078d3111b8c4803f88faa94a6f183ac60240a0ae010a1
RUN apk add --no-cache build-base python3 py3-pip py3-yaml \
    && python3 -m venv /opt/smwbuild \
    && /opt/smwbuild/bin/pip install --no-cache-dir pycdlib==1.14.0 PyYAML==6.0.2
ENV PATH="/opt/smwbuild/bin:${PATH}"
# No ROM, extracted game asset or source is copied into this image.
