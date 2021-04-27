# To make use of multiple cores during the compile stages of the docker build
# docker build -t aurora-deps-ubuntu --build-arg compile_cores=8 .

# By default one core is used to compile
ARG compile_cores=1

# Get MOOSE image
FROM helenbrooks/aurora-deps-ubuntu

RUN cd /home && \
    git clone https://github.com/aurora-multiphysics/aurora.git && \
    cd aurora/data && \
    tar xzvf endfb71_hdf5.tgz && \
    export OPENMC_CROSS_SECTIONS=/home/aurora/data/endfb71_hdf5/cross_sections.xml && \
    cd ../openmc && \
    make -j"$compile_cores" && \
    cd unit && \
    make -j"$compile_cores" && \
    ./run_tests && \
    cd ../../ && \
    make -j"$compile_cores" && \
    cd unit && \
    make -j"$compile_cores" && \
    ./run_tests
    
    