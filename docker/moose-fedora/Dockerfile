# Example usage
# docker build --no-cache -t moose-fedora -f docker/moose-fedora/Dockerfile --build-arg compile_cores=8 .

# Start from Fedora
FROM fedora:33

# Copy scripts across
RUN mkdir /home/scripts
COPY ./scripts/fedora-envs.sh /home/scripts
COPY ./scripts/setup-env-fedora.sh /home/scripts
COPY ./scripts/setup-python-venv.sh /home/scripts
COPY ./scripts/build-moose.sh /home/scripts

WORKDIR /home

# Get the packages we need
RUN ./scripts/setup-env-fedora.sh

# Set up python environment
RUN ./scripts/setup-python-venv.sh -w /home -d env -e moose -i /home/scripts/fedora-envs.sh -o /home/.moose_base_profile

# Optional build-arg to specify moose version and cores
ARG moose_tag=
ARG moose_git_sha=
ARG compile_cores=4

# Build MOOSE
RUN ./scripts/build-moose.sh -w /home -j "$compile_cores" -t "$moose_tag" -s "$moose_git_sha" -e /home/.moose_base_profile -o /home/.moose_profile -r