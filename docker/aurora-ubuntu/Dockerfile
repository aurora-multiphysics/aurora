# NB This docker file expects to be run from the root aurora directory, e.g.
# docker build -t aurora-ubuntu -f docker/aurora-ubuntu/Dockerfile .

# To make use of multiple cores during the compile stages of the docker build
# docker build -t aurora-ubuntu --build-arg compile_cores=8 \
#              -f docker/aurora-ubuntu/Dockerfile .

# To compile with gcov support and generate coverage report
# (Note if running locally, be sure to make clobber first, otherwise gcovr uses broken symlinks as a result of COPY command)
# docker build -t aurora-ubuntu --build-arg test_coverage=true --build-arg coveralls_token=$COVERALLS_TOKEN \
#              -f docker/aurora-ubuntu/Dockerfile .

ARG base_image_tag=latest

# Get image with aurora deps
FROM helenbrooks/aurora-deps-ubuntu:$base_image_tag

# By default 4 cores are used to compile
ARG compile_cores=4

# Should we generate a coverage report
ARG test_coverage=false

# Coverage repo token
ARG coveralls_token=""

# Branch name for coveralls
ARG coveralls_branch=""

# Copy entire repository into image
COPY ./ /home/aurora/

# Install extra packages if we want test coverage
RUN if "$test_coverage" ; then \
 			 /home/aurora/scripts/add-coveralls.sh -e /home/env/moose_profile; \
		fi

# Build
RUN if "$test_coverage" ; then \
        export CXXFLAGS="--coverage $CXXFLAGS"; \
        export CFLAGS="-fprofile-arcs -ftest-coverage"; \
        echo "Generating test coverage"; \
    fi && \
    cd /home/aurora && \
    ./scripts/build-aurora.sh -j "$compile_cores" \
    -e "/home/env/moose_profile,/home/env/openmc_profile" \
    -o /home/env/ \
    -d /home/aurora

# Generate test coverage report using coveralls and upload
RUN if "$test_coverage" ; then \
				cd /home/aurora; \
				./scripts/coverage-report.sh -e /home/env/moose_profile \
				-t "$coveralls_token" -b "$coveralls_branch"; \
    fi