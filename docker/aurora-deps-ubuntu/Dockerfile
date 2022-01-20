# To make use of multiple cores during the compile stages of the docker build
# docker build -t aurora-deps-ubuntu --build-arg compile_cores=8 .

# By default one core is used to compile
ARG compile_cores=1

# Get MOOSE image
FROM helenbrooks/moose-ubuntu:2021-04

# Get MOAB / Embree / DagMC dependencies
RUN apt-get update && apt-get -y install \
    libeigen3-dev \
    libhdf5-dev \
    libglfw3-dev \
    libtbb-dev \
    pkg-config

# Build MOAB
RUN mkdir /home/dagmc-bld
RUN cd /home/dagmc-bld && \
    git clone https://bitbucket.org/fathomteam/moab && \
    cd moab && \
    git checkout Version5.2.0 && \
    autoreconf -fi && \
    mkdir bld && \
    cd bld && \
    ../configure --prefix=/home/moab \
                --with-hdf5=/usr/lib/x86_64-linux-gnu/hdf5/serial/ \
                --enable-optimize \
                --enable-shared \
                --disable-debug && \
    make -j"$compile_cores" && \
    make check && \
    make install

# Build Embree
RUN cd /home/dagmc-bld && \
    git clone https://github.com/embree/embree.git && \
    cd embree && \
    git checkout v3.6.1 && \
    mkdir build && \
    cd build && \
    cmake ../ \
    -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_C_COMPILER=$CC \
    -DEMBREE_ISPC_SUPPORT=0 && \
    make -j"$compile_cores" && \
    make install

# Build DoubleDown
RUN cd /home/dagmc-bld && \
    git clone https://github.com/pshriwise/double-down && \
    cd double-down && \
    git checkout v1.0.0 && \
    mkdir build && \
    cd build && \
    cmake ../ \
    -DMOAB_DIR=/home/moab \
    -DEMBREE_DIR=/usr/local \
    -DCMAKE_INSTALL_PREFIX=/home/double-down && \
    make -j"$compile_cores" && \
    make install

# Build DagMC
RUN cd /home/dagmc-bld && \
    mkdir dagmc && \
    cd dagmc && \
    git clone https://github.com/svalinn/DAGMC && \
    cd DAGMC && \
    git checkout 0d07a744178af6275959c745fa4362d8b4d13559 && \
    cd ../ && \
    mkdir build && \
    cd build && \
    cmake ../DAGMC \
    -DMOAB_DIR=/home/moab \
    -DDOUBLE_DOWN=on \
    -DDOUBLE_DOWN_DIR=/home/double-down \
    -DBUILD_TALLY=ON \
    -DCMAKE_INSTALL_PREFIX=/home/dagmc && \
    make -j"$compile_cores" && \
    make test && \
    make install

# Clone and install NJOY2016
RUN cd /home/ && \
    git clone https://github.com/njoy/NJOY2016.git && \
    cd NJOY2016 && \
    mkdir build && \
    cd build && \
    cmake -Dstatic=on .. && \
    make && \
    make install

# Build OpenMC
RUN mkdir /home/openmc-bld && \
    cd /home/openmc-bld && \
    git clone https://github.com/openmc-dev/openmc.git && \
    cd openmc && \
    git checkout b62708f911d05e269e5c3083287e70d050ed35f9 && \
    cd ../ && \
    mkdir build && \
    cd build && \
    cmake ../openmc \
          -Doptimize=on \
          -Ddagmc=on \
          -DCMAKE_INSTALL_PREFIX=/home/openmc \
          -DDAGMC_DIR=/home/dagmc/lib/cmake && \
    make -j"$compile_cores" && \
    make -j"$compile_cores" install

ENV PATH=$PATH:/home/moab/bin:/home/openmc/bin:/home/dagmc/bin
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/moab/lib:/home/dagmc/lib
