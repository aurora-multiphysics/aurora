# Example usage
# docker build -t aurora-base-ubuntu -f docker/aurora-base-ubuntu/Dockerfile .

# Basic environment
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/London

# Copy files across
RUN mkdir /home/scripts
RUN mkdir /home/env
COPY ./scripts/setup-env-ubuntu.sh /home/scripts
COPY ./scripts/ubuntu-envs.sh /home/env

# Set working directory
WORKDIR /home

# Get the packages we need
RUN ./scripts/setup-env-ubuntu.sh
