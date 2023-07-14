#!/bin/bash

set -e

# We need to update package lists to get latest cmake
apt-get update
apt-get purge --auto-remove cmake
apt-get -y --fix-missing install \
        software-properties-common \
        lsb-release \
        gpg \
        wget
apt-get clean all
#wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - |  tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 6AF7F09730B3F0A4
apt-get update
apt-get install -y --fix-missing cmake \
        gcc \
        g++ \
        gfortran \
        git \
        libopenmpi3 \
        python3 \
        python3-venv \
        python3-dev \
        python3-distutils \
        python-is-python3 \
        rsync \
        bison \
        flex \
        libblas-dev \
        liblapack-dev \
        libhdf5-dev \
        libeigen3-dev \
        libglfw3-dev  \
        libtbb-dev  \
        pkg-config
