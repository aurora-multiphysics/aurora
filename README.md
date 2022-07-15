AURORA
=====

[![GitHub Actions build status](https://github.com/aurora-multiphysics/aurora/actions/workflows/main.yml/badge.svg?event=push)](https://github.com/aurora-multiphysics/aurora/actions/workflows/main.yml)
[![Coverage Status](https://coveralls.io/repos/github/aurora-multiphysics/aurora/badge.svg)](https://coveralls.io/github/aurora-multiphysics/aurora)

The [name](https://en.wikipedia.org/wiki/Aurora_(mythology)) of this application is based on the following acronym:

**A** **U**nified **R**esource for **O**penMC (fusion) **R**eactor **A**pplications.

AURORA combines the Monte Carlo neutron transport calculations from OpenMC with the FEA calculations supported by the MOOSE framework, intended for the modelling of tokamak physics. Currently supported modules within MOOSE are Heat Conduction and Tensor Mechanics, intended to model the increase in temperature arising from heat deposited from neutrons, and subsequent thermal expansion and density changes respectively. Support for further modules is ongoing.

For more information on MOOSE see: [http://mooseframework.org]

For more information on OpenMC see: [https://docs.openmc.org/en/stable/]

## License

[AURORA](https://github.com/aurora-multiphysics/aurora) is licensed under LGPL v2.1, see [here](https://github.com/aurora-multiphysics/aurora/blob/main/LICENSE).

## Installation

We currently provide support for Debian and Redhat flavours of Linux; we have explicitly tested for Ubuntu 20.04 and Fedora 33 using gcc 9.3.0.

If you would like support provided for another operating system or you experience any problems please get in touch via [GitHub Issues](https://github.com/aurora-multiphysics/aurora/issues), detailing your operating system using `lsb_release -a` (or equivalent), compiler version, and any relevant compiler errors.

If you do not want to build from source, please see the section on docker containers for a pre-built version.

### Installation from Source

First ensure you have all the dependencies installed, as outlined below. If you would like to install from source in a pre-built environment (i.e. you don't want to install the dependencies below), you may want to use one of our docker images (see section below).

#### A. Dependencies

1) Parallel dependencies (optional)
If you intend to run in parallel using either MPI or thread (or both) please ensure you have the following installed.
  a. MPI: OpenMPI / MPICH
  b. Threaded: OpenMP

2) OpenMC dependencies

  - a. HDF5
  
  On Debian:
```
apt-get install -y libhdf5-dev
```
  or Redhat
```
dnf -y install hdf5-devel
```

  - b. [MOAB](https://bitbucket.org/fathomteam/moab).
```
git clone https://bitbucket.org/fathomteam/moab && \
    cd moab && \
    git checkout Version5.2.0 && \
    autoreconf -fi && \
    mkdir bld && \
    cd bld && \
    ../configure --prefix=/home/moab \
                --with-hdf5=/PATH-TO-libhdf5.so/ \
                --enable-optimize \
                --enable-shared \
                --disable-debug && \
    make -j $compile_cores && \
    make check && \
    make install
```
  - c. [ (Optionally) Double Down](https://github.com/pshriwise/double-down) - if you want to leverage Intel Embree's ray tracing kernels within DAGMC. First install [Embree ](https://github.com/embree/embree) and its dependencies.
```
# Embree dependencies
sudo apt-get -y install libglfw3-dev libtbb-dev pkg-config

# Build Embree
git clone https://github.com/embree/embree.git && \
	cd embree && \
	git checkout v3.6.1 && \
	mkdir build && \
	cd build && \
	cmake ../ \
	-DCMAKE_CXX_COMPILER=$CXX \
	-DCMAKE_C_COMPILER=$CC \
	-DEMBREE_ISPC_SUPPORT=0 && \
	make -j $compile_cores && \
	make install 

# Build DoubleDown
git clone https://github.com/pshriwise/double-down && \
	cd double-down && \
	mkdir build && \
	cd build && \
	cmake ../ \
	-DMOAB_DIR=/home/moab \
	-DEMBREE_DIR=/PATH-TO-EMBREE/ \
	-DCMAKE_INSTALL_PREFIX=/INSTALL-PATH/ && \
	make -j $compile_cores && \
	make install
```

  - d. [DAGMC](https://svalinn.github.io/DAGMC/install/index.html)
Ensure to configure with Double Down if you want this library to be used.
```
mkdir dagmc-bld && \
    cd dagmc-bld && \
    git clone https://github.com/svalinn/DAGMC && \
    cd DAGMC && \
    git checkout develop && \
    cd ../ && \
    mkdir build && \
    cd build && \
    cmake ../DAGMC \
    -DMOAB_DIR=/PATH-TO-MOAB/ \
    -DDOUBLE_DOWN=on \
    -DDOUBLE_DOWN_DIR=/PATH-TO-DOUBLE-DOWN/ \
    -DBUILD_TALLY=ON \
    -DCMAKE_INSTALL_PREFIX=/INSTALL-PATH/ && \
    make -j $compile_cores && \
    make test && \
    make install
```

3) [OpenMC](https://docs.openmc.org/en/stable/)
```
mkdir openmc-bld && \
    cd openmc-bld && \
    git clone https://github.com/openmc-dev/openmc.git && \
    cd openmc && \
    git checkout develop && \
    cd ../ && \
    mkdir build && \
    cd build && \
    cmake ../openmc \
          -Doptimize=on \
          -Ddagmc=on \
          -DCMAKE_INSTALL_PREFIX=/INSTALL-PATH/ \
          -DDAGMC_DIR=/PATH-TO-DAGMC/ && \
    make -j $compile_cores && \
    make -j $compile_cores install
```  
  Further detailed installation instructions for OpenMC can be found [here](https://docs.openmc.org/en/stable/usersguide/install.html).
  Please ensure you configure with support for DagMC enabled, and support for MPI/threads enabled if you intend to run in parallel.

4) MOOSE ( + PETSc /  libMesh )
  Please follow these [installation instructions ](https://mooseframework.inl.gov/getting_started/installation/install_moose.html).
  
#### B. Environment

  The Makefile for AURORA assumes the following environment variables have been set:
 - `PETSC_DIR`: path to PETSc installation directory.
 - `MOOSE_DIR`: path to MOOSE installation directory.
Optionally you may want to set `MOOSE_JOBS` to the number of cores on your system.

In addition it is expected that the following MOAB / DAGMC / OpenMC executables can be found in your PATH: `openmc` ; `mbconvert`; `make_watertight`.

Finally, in order to run AURORA, you will need to have set the variable `OPENMC_CROSS_SECTIONS` to point to a cross_sections.xml file, see [here](https://docs.openmc.org/en/stable/usersguide/cross_sections.html#environment-variable) for more details.

#### C. Build
In case you skipped to this section, ensure you have set up your dependencies and environment as per sections A,B (or you are working in a pre-built docker container for these, see below).
``` 
git clone https://github.com/aurora-multiphysics/aurora.git && \
    cd aurora/openmc/ && \
    make -j $compile_cores && \
    cd unit && \
    make -j $compile_cores && \
    cd ../../ && \
    make -j $compile_cores && \
    cd unit && \
    make -j $compile_cores
```
There should now be an executable `aurora-opt` in the root aurora directory.

#### D. Run Tests
To check your installation works, it's a good idea to run the test suite.
Before you do this ensure the `OPENMC_CROSS_SECTIONS` environment variable points at a cross section file which contains KERMA-processed cross sections. For convenience we package a minimal set of cross sections needed for the tests, but you'll need to unpack them and set the `OPENMC_CROSS_SECTIONS` variable:
```
cd aurora/data && \
   tar xzvf endfb71_hdf5.tgz && \
   export OPENMC_CROSS_SECTIONS=/PATH-TO-AURORA/aurora/data/endfb71_hdf5/cross_sections.xml 
```
See [this discussion](https://openmc.discourse.group/t/nuclear-data-dependent-zero-result-for-heating-local-tally/833) for further details. Now you can run the tests as follows
```
cd aurora && \
./run_unit_tests
```
All the tests should pass. (Let us know via [GitHub Issues](https://github.com/aurora-multiphysics/aurora/issues) if you experience problems.)

### Docker

We appreciate that not all users will want to install the extensive list of dependencies upon which AURORA depends. Such users can simply download pre-built docker containers.
For those who prefer Ubunutu:
```
docker pull helenbrooks/aurora-ubuntu
```
...or for those who prefer Fedora:
```
docker pull helenbrooks/aurora-fedora
```
Then just `docker run` the image you pulled and you're good to go. (For those users not experienced with docker, take a look at this [tutorial](https://docs.docker.com/get-started/).)

Alternatively you may want to install AURORA from source, but with pre-built dependencies.
(e.g. perhaps you want to implement a new feature). In this case the relevant images are:
```
docker pull helenbrooks/aurora-deps-ubuntu
```
for Ubuntu users, or
```
docker pull helenbrooks/aurora-deps-fedora
```
for Fedora users. Spin up the image, and then follow the installation instructions from section C onwards.

## Usage / Examples

We recommend that you familiarise yourself with both OpenMC and MOOSE before trying AURORA, since AURORA (currently) depends on having input for both. Both OpenMC and MOOSE provide a number of examples [here](https://docs.openmc.org/en/stable/examples/index.html) and [here](https://mooseframework.inl.gov/getting_started/examples_and_tutorials/index.html).

Specifically, OpenMC requires that the following files exist in your run directory:
 - `settings.xml`
 - `materials.xml`
 - `tallies.xml`
 - `dagmc.h5m`

The `dagmc.h5m` file is a surface mesh file, in length units assumed to be cm. This [guide](https://svalinn.github.io/DAGMC/usersguide/trelis_workflow.html) provides details on the workflow to create such a file.

The AURORA application itself requires two input files, and an exodus file. For details on the syntax of these input files, see the following section.

The first input file controls the main driver application (which performs the FEA), and the second input file (referenced by the first) controls the sub-app that calls OpenMC. We recommend you have read [this guide](https://mooseframework.inl.gov/syntax/MultiApps/index.html) on multiapps before proceeding. 

Finally, the exodus file should contain a tetrahedral mesh of the geometry of interest. Currently this geometry needs to be the same as that in the dagmc.h5m file, however in future we intend to support the case where the FEA geometry is a subset of the OpenMC geometry. It is possible to use different lengthscales between the exodus and dagmc files, in which case the parameter `length_scale` for the  `MoabUserObject` should be set (e.g. to convert from m in the exodus file into cm in dagmc.h5m this parameter would be 100). Note that although MOOSE generically is unit-agnostic, it is necessary to have consistency with physical material properties. Thus, if you use m in your exodus file, ensure all length-dimensionful material properties are also given values in units of m. 

There are two examples provided in the root aurora directory. 
The first example performs a simple neutronics + heat conduction simulation.
```
./aurora-opt -i main.i
```
The second example performs neutronics + heat conduction + thermal expansion.
```
./aurora-opt -i main_temp_mech.i
```

## Input File Syntax

Please follow [this link](https://aurora-multiphysics.github.io/aurora/doc/htmldoc/index.html) to see documentation for input file syntax.

## API Documentation

Please follow [this link](https://aurora-multiphysics.github.io/aurora/doc/doxygen/html) to find code documentation for developers.

## Bugs / Problems / Feature Requests / Contact

If you experience any problems with the code, or find a bug, or would like a new feature implemented, please raise an issue on [GitHub](https://github.com/aurora-multiphysics/aurora/issues). If you think you know how to fix said issue and would like to do it yourself, please see the guidelines in the next section.

## Want to contribute?

If you would like to contribute to the code, please adhere to the following process.

1) [Open an issue](https://github.com/aurora-multiphysics/aurora/issues) on GitHub, detailing the change you would like to make. (This avoids multiple developers working on the same problem, and highlights your needs to us.)

2) Create a fork of the repository and clone onto your machine, see [here](https://docs.github.com/en/enterprise/2.13/user/articles/fork-a-repo#:~:text=A%20fork%20is%20a%20copy,point%20for%20your%20own%20idea).

3) Create a branch in the local copy of your fork.
```
git checkout -b <my_branch_name>
```

4) Make your changes, commit and push to your fork on GitHub.
```
git add < modified files>
git commit -m 'Here is a concise statement of changes'
git push
```
(If the changeset is large this is best split over several commits, one commit for each notable feature change).

5) Ensure that all tests pass. (See Run Tests section above.) If you have implemented a new feature, please ensure there is at least one unit test probing each public method of any new classes. It is good practice to ensure that pre-existing tests pass for each commit (unless noted otherwise in commit message).

6) Make sure your branch is up-to-date with any changes in the main branch of the aurora-multiphysics/aurora repository so that merging your changes is a smooth process. It is easy to stay in sync as follows:
- Configure a remote.
```
git remote add upstream https://github.com/aurora-multiphysics/aurora.git
```
(Check this worked with `git remote -v`.)
- Fetch upstream changes into your local fork:
```
git fetch upstream
```
- Switch to `main` branch (either `git stash` or commit your local changes first).
```
git switch main
```
- Merge the changes into your fork repository's `main` branch.
```
git merge upstream/main
```
- Switch back to your development branch.
```
git switch <my_branch_name>
```
- Finally, roll your changes on top of the changes from `main`. 
```
git rebase main
```
For a nice explanation of rebasing, see [here](https://git-scm.com/book/en/v2/Git-Branching-Rebasing). It's possible you may at this point have to deal with conflicts; manually edit the files which are in conflict to resolve them, then run:
```
git rebase --continue
```
It is a good idea to sync frequently so that your branch does not deviate significantly or merging will become quite a headache! Note, we will not approve pull requests that are not up-to-date with our `main` branch.

7) Update documentation (currently manual):
- Generate/update MOOSE markdown stubs
```
cd doc && ./moosedocs.py generate app_types OpenMCApp AuroraApp
```
- Build the MOOSE html pages
```
cd doc && ./moosedocs.py build --destination=htmldoc
```
- Build the doxgyen html pages
```
doxygen doc/doxygen/Doxyfile
```
- Commit changes.

8) Create a pull request, detailing the changes you have made and reference your original issue. Creating a pull request will trigger our CI pipeline, which tests the build and runs tests in two environments. If you would like to test your changes in a different environment to your own, you may want to use our docker images (see section on docker above).

Once a pull request has been opened we will review your code. We may require some changes, in which case you'll need to make some further commits. Once your code has been reviewd and approved, we will merge it into `main`.

