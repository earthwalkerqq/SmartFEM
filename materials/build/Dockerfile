FROM ubuntu: latest

RUN apk --no-cache --upgrade add \
    git \
    git-lfs \
    curl \
    jq \
    bash \
    build-base \
    python3 \
    valgrind \
    cppcheck \
    alpine-sdk \
    pcre-dev \
    clang-extra-tools \
    libomp \
    glut \
    glew \
    opengl \
    gmsh

RUN mkdir -p ./app

COPY ../.. /app

WORKDIR "/app"

# 