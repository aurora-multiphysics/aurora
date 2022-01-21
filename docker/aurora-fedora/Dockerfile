# To make use of multiple cores during the compile stages of the docker build
# docker build -t aurora-ubuntu --build-arg compile_cores=8 .

# To compile a particular git sha use
# docker build -t aurora-ubuntu --build-arg build_git_sha=${GITHUB_SHA} .

# Get MOOSE image with aurora deps
FROM helenbrooks/aurora-deps-fedora:v.0.2.1

# By default one core is used to compile
ARG compile_cores=1

# By default checkout main branch
ARG build_git_sha="main"

RUN cd /home && \
    git clone https://github.com/aurora-multiphysics/aurora.git && \
    cd aurora && \
    git checkout "$build_git_sha" && \
    cd data && \
    tar xzvf endfb71_hdf5.tgz && \
    export OPENMC_CROSS_SECTIONS=/home/aurora/data/endfb71_hdf5/cross_sections.xml && \
    cd ../openmc/unit && \
    make -j"$compile_cores" && \
    cd ../../unit && \
    make -j"$compile_cores" && \
    ./run_tests
