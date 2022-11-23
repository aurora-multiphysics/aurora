# Example usage
# docker build --no-cache -t aurora-deps-fedora -f docker/aurora-deps-fedora/Dockerfile --build-arg compile_cores=8 .

# Base image
FROM helenbrooks/aurora-base-fedora:latest

# Copy files across
COPY ./scripts/build-aurora-deps.sh /home/scripts/build-aurora-deps.sh
COPY ./scripts/build-package.sh /home/scripts/build-package.sh

# By default 4 cores are used to compile
ARG compile_cores=4

# Create a local directory to install packages
RUN mkdir local


# Build all AURORA dependencies
RUN cd /home/scripts  && \
    HDF5_DIR=$(rpm -ql hdf5-devel | grep libhdf5.so | sed 's/\/libhdf5.so//') \
    ./build-aurora-deps.sh -w /home/ \
    -j "$compile_cores" \
    -i /home/local  \
    -e /home/env/fedora-envs.sh \
    -o /home/env/
