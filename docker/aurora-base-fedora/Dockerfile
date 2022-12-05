# Example usage
# docker build -t aurora-base-fedora -f docker/aurora-base-fedora/Dockerfile .

# Start from Fedora
FROM fedora:33

# Copy files across
RUN mkdir /home/scripts
RUN mkdir /home/env
COPY ./scripts/setup-env-fedora.sh /home/scripts
COPY ./scripts/fedora-envs.sh /home/env

# Set working directory
WORKDIR /home

# Get the packages we need
RUN ./scripts/setup-env-fedora.sh