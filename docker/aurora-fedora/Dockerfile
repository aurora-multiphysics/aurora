# NB This docker file expects to be run from the root aurora directory, e.g. 
# docker build -t aurora-fedora -f docker/aurora-fedora/Dockerfile .

# To make use of multiple cores during the compile stages of the docker build
# docker build -t aurora-fedora --build-arg compile_cores=8 \
#              -f docker/aurora-fedora/Dockerfile .

ARG base_image_tag=latest

# Get image with aurora deps
FROM helenbrooks/aurora-deps-fedora:$base_image_tag

# By default four cores are used to compile
ARG compile_cores=4

# Copy entire repository into image
COPY ./ /home/aurora/

RUN  cd /home/aurora && \
    ./scripts/build-aurora.sh -j "$compile_cores" \
    -e "/home/env/moose_profile,/home/env/openmc_profile" \
    -o /home/env/ \
    -d /home/aurora