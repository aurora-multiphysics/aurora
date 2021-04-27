FROM ubuntu:20.04
# Basic environment
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/London
RUN apt-get update && apt-get -y install \
    cmake \
    gcc \
    g++ \
    gfortran \
    git \
    libopenmpi3 \
    python3 \
    python3-dev \
    python3-distutils \
    python-is-python3 \
    wget
# PETSC dependencies
RUN apt-get -y install \
    bison \
    flex \
    libblas-dev \
    liblapack-dev
# MPI environment variables
ENV CC=mpicc
ENV CXX=mpicxx
ENV F90=mpif90
ENV F77=mpif77
ENV FC=mpif90
# Get MOOSE source code
RUN cd /home/ && \
    git clone https://github.com/idaholab/moose.git && \
    cd moose && \
    git checkout master
ENV MOOSE_JOBS=4
# Make PETSC
RUN cd /home/moose && \
    ./scripts/update_and_rebuild_petsc.sh --prefix=/home/petsc
ENV PETSC_DIR=/home/petsc
# Make libMesh
RUN cd /home/moose && \
    ./scripts/update_and_rebuild_libmesh.sh --with-mpi
# Make MOOSE framework
RUN cd /home/moose/ && \
    ./configure --with-derivative-size=200 --with-ad-indexing-type=global && \
    cd test && \
    make -j4
# This is needed or it mpiexec complains because docker runs as root
# Discussion on this issue https://github.com/open-mpi/ompi/issues/4451
ENV OMPI_ALLOW_RUN_AS_ROOT=1
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1
RUN cd /home/moose/test && \
    ./run_tests -j 4
ENV MOOSE_DIR=/home/moose
# Make MOOSE modules
RUN cd /home/moose/modules && \
    make -j4  && \
    ./run_tests -j 4
# Unset these variables we set before
ENV OMPI_ALLOW_RUN_AS_ROOT=
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=