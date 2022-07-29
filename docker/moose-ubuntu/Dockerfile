# Basic usage:
# docker build -t moose-ubuntu .
# Build with tag
# docker build -t moose-ubuntu:tagname --build-arg moose_tag=tag .
# Build with git sha
# docker build -t moose-ubuntu:tagname --build-arg moose_git_sha=gitsha .
# Build with particular configuration arguments
# docker build -t moose-ubuntu:tagname --build-arg moose_configure_flags=config .

# Basic environment
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/London

# We need to update package lists to get latest cmake
RUN apt-get update && \
    apt-get -y install \
        software-properties-common \
        lsb-release && \
    apt-get clean all && \
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null && \
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 6AF7F09730B3F0A4 && \
    apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" && \
    apt-get update && \
    apt-get install -y kitware-archive-keyring && \
    rm /etc/apt/trusted.gpg.d/kitware.gpg && \
    apt-get install -y cmake \
            gcc \
            g++ \
            gfortran \
            git \
            libopenmpi3 \
            python3 \
            python3-dev \
            python3-distutils \
            python-is-python3 \
            rsync

# PETSC dependencies
RUN apt-get -y install \
    bison \
    flex \
    libblas-dev \
    liblapack-dev \
    libhdf5-dev

# MPI environment variables
ENV CC=mpicc
ENV CXX=mpicxx
ENV F90=mpif90
ENV F77=mpif77
ENV FC=mpif90

# Optional build-arg to specify moose version
ARG moose_tag=
ARG moose_git_sha=

# Get MOOSE source code and checkout the desired version
RUN cd /home/ && \
    git clone https://github.com/idaholab/moose.git && \
    cd moose && \
    if ! [ "$moose_tag" = "" ] ; then \
       git fetch --tags && \
       git checkout tags/"$moose_tag" -b master; \
    elif ! [ "$moose_git_sha" = "" ] ; then \
       git checkout "$moose_git_sha"; \
    else \
       git checkout master; \
    fi && \
    git log -1

# Specify number of cores for compilation
ARG compile_cores=4
ENV MOOSE_JOBS=$compile_cores

# Tell PETSC where to find HDF5
ENV HDF5_DIR=/usr/lib/x86_64-linux-gnu/hdf5/serial/

# Make PETSC
RUN cd /home/moose && \
    ./scripts/update_and_rebuild_petsc.sh --prefix=/home/petsc \
                                          --CC=$CC \
                                          --CXX=$CXX \
                                          --FC=$FC

ENV PETSC_DIR=/home/petsc

# Make libMesh
RUN cd /home/moose && \
    METHODS="opt" ./scripts/update_and_rebuild_libmesh.sh --with-mpi

# Allow specification of MOOSE configuration flags
ARG moose_configure_flags=""

RUN echo "$moose_configure_flags"

# Make MOOSE framework
RUN cd /home/moose/ && \
    ./configure "$moose_configure_flags"  && \
    cd test && \
    METHOD="opt" make -j"$compile_cores"

# Make MOOSE modules
RUN cd /home/moose/modules && \
    make -j"$compile_cores"

ENV MOOSE_DIR=/home/moose

# This is needed or it mpiexec complains because docker runs as root
# Discussion on this issue https://github.com/open-mpi/ompi/issues/4451
ENV OMPI_ALLOW_RUN_AS_ROOT=1
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

# RUN tests
RUN cd /home/moose/test && \
    ./run_tests -j "$compile_cores"

#RUN cd /home/moose/modules && \
#    ./run_tests -j "$compile_cores"

# Unset these variables we set before
ENV OMPI_ALLOW_RUN_AS_ROOT=
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=