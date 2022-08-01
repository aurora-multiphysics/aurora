# To make use of multiple cores during the compile stages of the docker build
# docker build -t aurora-deps-ubuntu --build-arg compile_cores=8 .

# Start from Fedora
FROM fedora:33

# By default one core is used to compile
ARG compile_cores=4

RUN echo "Building with $compile_cores cores"

# Get the packages we need
RUN dnf -y install cmake \
    git gcc gcc-c++ gcc-gfortran \
    hdf5-devel blas-devel lapack-devel \
    wget eigen3-devel \
    openmpi-devel mpich mpich-devel \
    autoconf automake libtool vim emacs \
    bison flex bison-devel flex-devel \
    python3 python3-devel libtirpc libtirpc-devel

#  Set environment vars
ENV MPI_BIN /usr/lib64/mpich/bin
ENV MPI_SYSCONFIG /etc/mpich-x86_64
ENV MPI_FORTRAN_MOD_DIR /usr/lib64/gfortran/modules/mpich
ENV MPI_INCLUDE /usr/include/mpich-x86_64
ENV MPI_LIB /usr/lib64/mpich/lib
ENV MPI_MAN /usr/share/man/mpich-x86_64
ENV MPI_PYTHON_SITEARCH %{python2_sitearch}/mpich
ENV MPI_PYTHON2_SITEARCH %{python2_sitearch}/mpich
ENV MPI_PYTHON3_SITEARCH /usr/lib64/python3.9/site-packages/mpich
ENV MPI_COMPILER mpich-x86_64
ENV MPI_SUFFIX _mpich
ENV MPI_HOME /usr/lib64/mpich
ENV PATH /usr/lib64/mpich/bin:$PATH
ENV LD_LIBRARY_PATH /usr/lib64/mpich/lib:$LD_LIBRARY_PATH
ENV MANPATH /usr/share/man/mpich-x86_64
ENV PKG_CONFIG_PATH /usr/lib64/mpich/lib/pkgconfig
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

RUN cd /home/moose && \
    git submodule update --init --recursive petsc

ENV PETSC_ARCH=arch-moose
ENV PETSC_DIR=/home/moose/petsc

# Configure PETSC ourselves because MOOSE script can't find HDF5 on Fedora
RUN cd /home/moose/petsc && \
    ./configure --download-hypre=1 \
                --prefix=/home/petsc \
                --CC=$CC \
                --CXX=$CXX \
                --FC=$FC \
                --with-shared-libraries=1 \
                --with-hdf5=1 \
                --with-make-np="$MOOSE_JOBS" \
                --with-debugging=no \
                --download-fblaslapack=1 \
                --download-metis=1 \
                --download-ptscotch=1 \
                --download-parmetis=1 \
                --download-superlu_dist=1 \
                --download-mumps=1 \
                --download-strumpack=1 \
                --download-scalapack=1 \
                --download-slepc=1 \
                --with-mpi=1 \
                --with-openmp=1 \
                --with-cxx-dialect=C++11 \
                --with-fortran-bindings=0 \
                --with-sowing=0 \
                --with-64-bit-indices

RUN cd /home/moose/petsc && \
    make all && \
    make install

ENV PETSC_DIR=/home/petsc

# Make libMesh
RUN cd /home/moose && \
    METHODS="opt" ./scripts/update_and_rebuild_libmesh.sh --with-mpi

# Allow specification of MOOSE configuration flags
ARG moose_configure_flags=""

# Make MOOSE framework
RUN cd /home/moose/ && \
    ./configure "$moose_configure_flags"  && \
    cd test && \
    METHOD="opt" make -j"$compile_cores"

# Make MOOSE modules
RUN cd /home/moose/modules && \
    make -j"$compile_cores"

ENV MOOSE_DIR=/home/moose

# We need rsync for the tests
RUN dnf -y install rsync

# Run tests
RUN cd /home/moose/test && \
    ./run_tests -j "$compile_cores"

RUN cd /home/moose/modules && \
    ./run_tests -j "$compile_cores"

    