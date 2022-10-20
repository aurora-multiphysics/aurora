# Basic usage:
# docker build -t moose-ubuntu .
# Build with tag
# docker build -t moose-ubuntu:tagname --build-arg moose_tag=tag .
# Build with git sha
# docker build -t moose-ubuntu:tagname --build-arg moose_git_sha=gitsha .
# Build with particular configuration arguments

# Basic environment
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/London

RUN mkdir /home/scripts

COPY ./scripts/setup-env-ubuntu.sh /home/scripts
COPY ./scripts/ubuntu-envs.sh /home/scripts
COPY ./scripts/build-moose.sh /home/scripts

WORKDIR /home

# Get the packages we need
RUN ./scripts/setup-env-ubuntu.sh

# Optional build-arg to specify moose version
ARG moose_tag=
ARG moose_git_sha=
ARG compile_cores=4


ENV PETSC_DIR=/home/petsc

RUN ./scripts/build-moose.sh -w /home -j "$compile_cores" -t "$moose_tag" -s "$moose_git_sha" -d /usr/lib/x86_64-linux-gnu/hdf5/serial/ -e "scripts/ubuntu-envs.sh"  -o /home/.moose_profile -r