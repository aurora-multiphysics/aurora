# NB This docker file expects to be run from the root aurora directory, e.g.
# docker build -t aurora-ubuntu -f docker/aurora-ubuntu/Dockerfile .

# To make use of multiple cores during the compile stages of the docker build
# docker build -t aurora-ubuntu --build-arg compile_cores=8 \
#              -f docker/aurora-ubuntu/Dockerfile .

# To compile with gcov support and generate coverage report
# (Note if running locally, be sure to make clobber first, otherwise gcovr uses broken symlinks as a result of COPY command)
# docker build -t aurora-ubuntu --build-arg test_coverage=true --build-arg coveralls_token=$COVERALLS_TOKEN \
#              -f docker/aurora-ubuntu/Dockerfile .

# Get MOOSE image with aurora deps
FROM helenbrooks/aurora-deps-ubuntu:v.0.3.0

# By default one core is used to compile
ARG compile_cores=1

# Should we generate a coverage report
ARG test_coverage=false

# Coverage repo token
ARG coveralls_token=""

# Branch name for coveralls
ARG coveralls_branch=""

# Install extra packages if we want test coverage
RUN if "$test_coverage" ; then \
       apt-get update && \
       apt-get install -y python3-pip && \
       pip install cpp-coveralls && \
       pip install coveralls && \
       pip install gcovr; \
    fi

# Copy entire repository into image
COPY ./ /home/aurora/

# Build
RUN if "$test_coverage" ; then \
       export CXXFLAGS="--coverage $CXXFLAGS"; \
       export CFLAGS="-fprofile-arcs -ftest-coverage"; \
       echo "Generating test coverage"; \
    fi && \
    cd /home/aurora/data && \
    tar xzvf endfb71_hdf5.tgz && \
    export OPENMC_CROSS_SECTIONS=/home/aurora/data/endfb71_hdf5/cross_sections.xml && \
    cd ../openmc/unit && \
    make -j"$compile_cores"  && \
    cd ../../unit && \
    make -j"$compile_cores" && \
    ./run_tests

# Generate test coverage report using coveralls and upload
RUN if "$test_coverage" && "$coveralls_token" != "" ; then \
       echo "Uploading test coverage to coveralls"; \
       export COVERALLS_REPO_TOKEN="$coveralls_token" && \
       export CI_BRANCH="$coveralls_branch" && \
       cd /home/aurora/ && \
       cpp-coveralls -i src/ -i include/ -i openmc/src/ -i openmc/include/ --exclude-pattern "/usr/*" --dump cpp_cov.json && \
       coveralls --merge=cpp_cov.json; \
    elif  "$test_coverage"  ; then \
       echo "Test coverage set to true but no coveralls token, using govr" && \
       cd /home/aurora/ && \
       gcovr -e src/main.C -e openmc/unit/ -e unit/ -e openmc/src/main.C -e test -e openmc/test build/ openmc/build/; \
    fi
