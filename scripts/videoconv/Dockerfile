# The libdragon build image plus ffmpeg, which videoconv64 drives to encode
# the Game info screen's preview clips (scripts/make_preview_clip.py).
FROM ghcr.io/dragonminded/libdragon:latest
RUN apt-get update \
 && apt-get install -y --no-install-recommends ffmpeg \
 && rm -rf /var/lib/apt/lists/*
