#!/bin/bash

set -e

TAG=v.0.3.2-pre
JOBS=64
RELEASE=0

# Ubuntu build
docker build --no-cache -t helenbrooks/aurora-base-ubuntu:$TAG -f docker/aurora-base-ubuntu/Dockerfile .
docker build --no-cache -t helenbrooks/moose-ubuntu:$TAG -f docker/moose-ubuntu/Dockerfile --build-arg base_image_tag=$TAG  --build-arg compile_cores=$JOBS .
docker build --no-cache -t helenbrooks/aurora-deps-ubuntu:$TAG -f docker/aurora-deps-ubuntu/Dockerfile --build-arg base_image_tag=$TAG --build-arg moose_image_tag=$TAG --build-arg compile_cores=$JOBS .
docker build --no-cache -t helenbrooks/aurora-ubuntu:$TAG -f docker/aurora-ubuntu/Dockerfile --build-arg base_image_tag=$TAG --build-arg compile_cores=$JOBS .

# Fedora build
docker build --no-cache -t helenbrooks/aurora-base-fedora:$TAG -f docker/aurora-base-fedora/Dockerfile .
docker build --no-cache -t helenbrooks/moose-fedora:$TAG -f docker/moose-fedora/Dockerfile --build-arg base_image_tag=$TAG  --build-arg compile_cores=$JOBS .
docker build --no-cache -t helenbrooks/aurora-deps-fedora:$TAG -f docker/aurora-deps-fedora/Dockerfile --build-arg base_image_tag=$TAG --build-arg moose_image_tag=$TAG --build-arg compile_cores=$JOBS .
docker build --no-cache -t helenbrooks/aurora-fedora:$TAG -f docker/aurora-fedora/Dockerfile --build-arg base_image_tag=$TAG --build-arg compile_cores=$JOBS .

# If we got to here we should push all images
# Ubuntu build
docker push helenbrooks/aurora-base-ubuntu:$TAG
docker push helenbrooks/moose-ubuntu:$TAG
docker push helenbrooks/aurora-deps-ubuntu:$TAG
docker push helenbrooks/aurora-ubuntu:$TAG
# Fedora build
docker push helenbrooks/aurora-base-fedora:$TAG
docker push helenbrooks/moose-fedora:$TAG
docker push helenbrooks/aurora-deps-fedora:$TAG
docker push helenbrooks/aurora-fedora:$TAG

# If this is a release version, then tag as latest
if [ $RELEASE -eq 1 ] ; then
    # Tag as latest
    docker tag helenbrooks/aurora-base-ubuntu:$TAG helenbrooks/aurora-base-ubuntu:latest
    docker tag helenbrooks/moose-ubuntu:$TAG helenbrooks/moose-ubuntu:latest
    docker tag helenbrooks/aurora-deps-ubuntu:$TAG helenbrooks/aurora-deps-ubuntu:latest
    docker tag helenbrooks/aurora-ubuntu:$TAG helenbrooks/aurora-ubuntu:latest
    docker tag helenbrooks/aurora-base-fedora:$TAG helenbrooks/aurora-base-fedora:latest
    docker tag helenbrooks/moose-fedora:$TAG helenbrooks/moose-fedora:latest
    docker tag helenbrooks/aurora-deps-fedora:$TAG helenbrooks/aurora-deps-fedora:latest
    docker tag helenbrooks/aurora-fedora:$TAG helenbrooks/aurora-fedora:latest
    # Push
    docker push helenbrooks/aurora-base-ubuntu:latest
    docker push helenbrooks/moose-ubuntu:latest
    docker push helenbrooks/aurora-deps-ubuntu:latest
    docker push helenbrooks/aurora-ubuntu:latest
    docker push helenbrooks/aurora-base-fedora:latest
    docker push helenbrooks/moose-fedora:latest
    docker push helenbrooks/aurora-deps-fedora:latest
    docker push helenbrooks/aurora-fedora:latest
fi
