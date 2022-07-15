# NB This docker file expects to be run from the root aurora directory, e.g. 
# docker build -t aurora-fedora -f docker/aurora-fedora/Dockerfile .

# To make use of multiple cores during the compile stages of the docker build
# docker build -t aurora-fedora --build-arg compile_cores=8 \
#              -f docker/aurora-fedora/Dockerfile .

# Get MOOSE image with aurora deps
FROM helenbrooks/aurora-deps-fedora:v.0.3.0

# By default one core is used to compile
ARG compile_cores=1

# Copy entire repository into image
COPY ./ /home/aurora/

RUN cd /home/aurora/data && \
    tar xzvf endfb71_hdf5.tgz && \
    export OPENMC_CROSS_SECTIONS=/home/aurora/data/endfb71_hdf5/cross_sections.xml && \
    cd ../openmc/unit && \
    make -j"$compile_cores" && \
    cd ../../unit && \
    make -j"$compile_cores" && \
    ./run_tests
