# Example usage
# docker build --no-cache -t moose-ubuntu -f docker/moose-ubuntu/Dockerfile --build-arg compile_cores=8 .

ARG base_image_tag=latest

# Get base image
FROM helenbrooks/aurora-base-ubuntu:$base_image_tag

# Copy files across
COPY ./scripts/setup-python-venv.sh /home/scripts
COPY ./scripts/build-moose.sh /home/scripts

# Set up python environment
RUN ./scripts/setup-python-venv.sh -w /home -d env -e moose -i /home/env/ubuntu-envs.sh -o /home/env/moose_base_profile

# Optional build-arg to specify moose version and cores
ARG moose_tag=
ARG moose_git_sha=
ARG compile_cores=4

# Build MOOSE
RUN ./scripts/build-moose.sh -w /home -j "$compile_cores" -t "$moose_tag" -s "$moose_git_sha" -d /usr/lib/x86_64-linux-gnu/hdf5/serial/ -e /home/env/moose_base_profile  -o /home/env/moose_profile -f "" -r > /home/build_moose.log 2>&1