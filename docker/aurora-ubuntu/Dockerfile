# To make use of multiple cores during the compile stages of the docker build
# docker build -t aurora-ubuntu --build-arg compile_cores=8 .

# To compile a particular git sha use
# docker build -t aurora-ubuntu --build-arg build_git_sha=${GITHUB_SHA} .

# To compile with gcov support and generate coverage report
# docker build -t aurora-ubuntu --build-arg test_coverage=true .

# Get MOOSE image with aurora deps
FROM helenbrooks/aurora-deps-ubuntu:v.0.2.1

# By default one core is used to compile
ARG compile_cores=1

# By default checkout main branch
ARG build_git_sha="main"

# Should we generate a coverage report
ARG test_coverage=false

# Coverage repo token
ARG coveralls_token=""

# Branch name for coveralls
ARG coveralls_branch=""

# Install pip and gcovr if we want test coverage
RUN if "$test_coverage" ; then \
       apt-get install -y python3-pip && \
       pip install cpp-coveralls && \
       pip install coveralls; \
    fi

# Build
RUN if "$test_coverage" ; then \
       export CXXFLAGS="--coverage $CXXFLAGS"; \
       export CFLAGS="-fprofile-arcs -ftest-coverage"; \
       echo "Generating test coverage"; \
    fi && \
    cd /home && \
    git clone https://github.com/aurora-multiphysics/aurora.git && \
    cd aurora && \
    git checkout "$build_git_sha" && \
    cd data && \
    tar xzvf endfb71_hdf5.tgz && \
    export OPENMC_CROSS_SECTIONS=/home/aurora/data/endfb71_hdf5/cross_sections.xml && \
    cd ../openmc/unit && \
    make -j"$compile_cores" && \
    cd ../../unit && \
    make -j"$compile_cores" && \
    ./run_tests

# Generate test coverage report using coveralls and upload
RUN if "$test_coverage" ; then \
       export COVERALLS_REPO_TOKEN="$coveralls_token" && \
       export CI_BRANCH="$coveralls_branch" && \
       cd /home/aurora/ && \
       cpp-coveralls -i src/ -i include/ -i openmc/src/ -i openmc/include/ --exclude-pattern "/usr/*" --dump cpp_cov.json && \
       coveralls --merge=cpp_cov.json; \
    fi
