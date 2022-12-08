# Example usage
# docker build --no-cache -t moose-fedora -f docker/moose-fedora/Dockerfile --build-arg compile_cores=8 .

# Get base image
FROM helenbrooks/aurora-base-fedora:latest

# Copy scripts across
COPY ./scripts/setup-python-venv.sh /home/scripts
COPY ./scripts/build-moose.sh /home/scripts

WORKDIR /home

# Set up python environment
RUN ./scripts/setup-python-venv.sh -w /home -d env -e moose -i /home/env/fedora-envs.sh -o /home/env/moose_base_profile

# Optional build-arg to specify moose version and cores
ARG moose_tag=
ARG moose_git_sha=
ARG compile_cores=4

# Build MOOSE
RUN export HDF5_DIR=$(rpm -ql hdf5-devel | grep libhdf5.so | sed 's/\/libhdf5.so//') && \
		./scripts/build-moose.sh -w /home -j "$compile_cores" -t "$moose_tag" -s "$moose_git_sha" -d ${HDF_DIR} -e /home/env/moose_base_profile -o /home/env/moose_profile  -f "" -r