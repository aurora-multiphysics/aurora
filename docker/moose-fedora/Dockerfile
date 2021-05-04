# To make use of multiple cores during the compile stages of the docker build
# docker build -t aurora-deps-ubuntu --build-arg compile_cores=8 .

# Start from Fedora
FROM fedora:33

# By default one core is used to compile
ARG compile_cores=1

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
ENV MOOSE_JOBS="$compile_cores"

# Get MOOSE source
RUN cd /home/ && \
    git clone https://github.com/idaholab/moose.git && \
    cd moose && \
    git checkout master

# Make PETSC
RUN cd /home/moose &&  \
    ./scripts/update_and_rebuild_petsc.sh --prefix=/home/petsc
ENV PETSC_DIR=/home/petsc

# Make libMesh
RUN cd /home/moose && \
    ./scripts/update_and_rebuild_libmesh.sh --with-mpi

# Make MOOSE framework
RUN cd /home/moose/ && \
    ./configure --with-derivative-size=200 --with-ad-indexing-type=global && \
    cd test && \
    make -j"$compile_cores" && \
     ./run_tests -j"$compile_cores"
ENV MOOSE_DIR=/home/moose

# Make MOOSE modules
RUN cd /home/moose/modules && \
    make -j"$compile_cores"  && \
    ./run_tests -j"$compile_cores"

    