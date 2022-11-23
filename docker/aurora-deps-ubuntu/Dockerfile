# Example usage
# docker build --no-cache -t aurora-deps-ubuntu -f docker/aurora-deps-ubuntu/Dockerfile --build-arg compile_cores=8 .

# Base image
FROM helenbrooks/aurora-base-ubuntu:latest

# Copy files across
COPY ./scripts/build-aurora-deps.sh /home/scripts/build-aurora-deps.sh
COPY ./scripts/build-package.sh /home/scripts/build-package.sh

# By default 4 cores are used to compile
ARG compile_cores=4

# Create a local directory to install packages
RUN mkdir local

# Build all AURORA dependencies
RUN cd /home/scripts  && \
    HDF5_DIR=/usr/lib/x86_64-linux-gnu/hdf5/serial/ \
    ./build-aurora-deps.sh -w /home/ \
    -j "$compile_cores" \
    -i /home/local  \
    -e /home/env/ubuntu-envs.sh \
    -o /home/env/