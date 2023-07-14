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

## Citation

If you have used AURORA in your work to produce results, please cite:

- _H Brooks and A Davis 2023 Plasma Phys. Control. Fusion 65 024002_

  **DOI:** [10.1088/1361-6587/aca998](https://iopscience.iop.org/article/10.1088/1361-6587/aca998)

  **Bibtex:**
  ```
  @article{brooks_aurora,
  author = {Brooks, H and Davis, A},
  year = {2022},
  month = {12},
  pages = {024002},
  title = {Scalable multi-physics for fusion reactors with AURORA},
  volume = {65},
  journal = {Plasma Physics and Controlled Fusion},
  doi = {10.1088/1361-6587/aca998}
  }
  ```

If in addition you are performing breeder blanket studies and/or you have used the code [Achlys](https://github.com/aurora-multiphysics/achlys), please also cite:

- _Brooks H, Dixon S and Davis A 2022 Towards Multiphysics Simulations of Fusion Breeder Blankets International Conference on Physics of Reactors 2022 (PHYSOR 2022) / Pittsburgh, PA, May 15-20 (American Nuclear Society) pp 2480–2489_

  **Bibtex:**
  ```
  @inproceedings{brooks_physor,
  author       = "Brooks, Helen and Dixon, Stephen, and Davis, Andrew",
  title        = {{Towards Multiphysics Simulations of Fusion Breeder Blankets}},
  booktitle    = {{International Conference on Physics of Reactors 2022 (PHYSOR 2022) / Pittsburgh, PA, May 15-20}},
  year         = "2022",
  pages        = "2480-2489",
  publisher    = "American Nuclear Society",
  }
  ```

We also recommend that you cite works corresponding to AURORA's primary dependencies: [MOOSE](https://mooseframework.inl.gov/citing.html) and [OpenMC](https://docs.openmc.org/en/stable/publications.html).

## Installation

We currently provide support for Debian and Redhat flavours of Linux; we have explicitly tested for Ubuntu 20.04 and Fedora 33 using gcc 9.3.0.

If you would like support provided for another operating system or you experience any problems please get in touch via [GitHub Issues](https://github.com/aurora-multiphysics/aurora/issues), detailing your operating system using `lsb_release -a` (or equivalent), compiler version, and any relevant compiler errors.

If you do not want to build from source, please see the section on docker containers for a pre-built version.

### Installation from Source

First ensure you have all the dependencies installed, as outlined below. If you would like to install from source in a pre-built environment (i.e. you don't want to install the dependencies below), you may want to use one of our docker images (see section below).

Otherwise, start by cloning the repository. All subsequent commands, unless otherwise indicated, are assumed to be executed from the root aurora directory:
```
git clone https://github.com/aurora-multiphysics/aurora.git && \
cd aurora/
```

#### A. Install Dependencies

##### 1. Install recommended packages (requires sudo permissions)

In the case of Ubuntu run from the root AURORA directory:
```
sudo ./scripts/setup-env-ubuntu.sh
```
or for Fedora run
```
sudo ./scripts/setup-env-fedora.sh
```
For HPC systems where administrative permissions is not typical, it is likely you can skip this step, since assumed packages will likely already be available for example as modules. Subsequent steps assume the following packages are available in your environment:
- git
- cmake
- C++ compiler
- Fortran compiler
- Python 3
- HDF5
- BLAS
- LAPACK
- Eigen
- OpenMPI / MPICH (Optional, but recommended)
- rsync
- bison
- flex
- TBB (Optional, needed for Embree)
- GLFW (Optional, needed for Embree)

##### 2. Install python packages

There are a number of python packages required to run the MOOSE test suite. We recommend installing these in a python virtual environment, using the script we have provided. For a full list of options run:
```
./scripts/setup-python-venv.sh -h
```
Although the flags are optional and have sensible defaults, we recommend most users should run with the following options:
```
./scripts/setup-python-venv.sh -w $WORKDIR -d $ENV -e $NAME -i $BASE_PROFILE -o $PROFILE
```
where:
- WORKDIR is the root level directory where to run commands
- ENV is the directory name in WORKDIR where to create the moose python environment
- BASE_PROFILE is a file from which to install the base environment (e.g. module load commands for HPC)
- PROFILE is an output profile to source to set up the MOOSE environment in future

##### 3. Install MOOSE

We recommend building MOOSE using the script we have provided. For a full list of options run:
```
./scripts/build-moose.sh -h
```
Although the flags are optional and have sensible defaults, we recommend most users run with the following options:
```
./scripts/build-moose.sh -w $WORKDIR -j $JOBS  -d $HDF5DIR -e ${BASE_PROFILE}  -o $PROFILE
```
where
- WORKDIR is a directory in which to install moose
- JOBS is the number of cores to compile with
- HDF5DIR is the path to an installation of HDF5
- BASE_PROFILE is a file from which to install the base environment (e.g. module load commands for HPC)
- PROFILE is an output profile to source to set up the MOOSE environment in future

Please note, this script assumes:
- the environment variables CC, CXX and FC have been set to point at the compilers you wish to use. We recommend this is provided in your base profile.
- the python packages from part (A) of this step have been installed (these are necessary for the MOOSE test suite). If you used our script, remember to source the output profile which was created in that step. We recommend putting this command in your base profile.

##### 4. Install additional dependencies
In adddition to MOOSE, AURORA has the following dependencies.
  - [MOAB](https://bitbucket.org/fathomteam/moab)
  - [Embree](https://github.com/embree/embree.git) (optional)
  - [DoubleDownDouble Down](https://github.com/pshriwise/double-down) (optional)
  - [DAGMC](https://svalinn.github.io/DAGMC/install/index.html)
  - [OpenMC](https://docs.openmc.org/en/stable/)

We recommend doing this with the script provided. First move into the scripts directory.
```
cd scripts
```
For a full list of options run:
```
./build-aurora-deps.sh -h
```
Although the flags are optional and have sensible defaults, we recommend most users run with the following options:
```
./build-aurora-deps.sh -w $WORKDIR -j $JOBS -i $INSTALLDIR -e $BASE_PROFILE -o $OUTPUTDIR
```
where
- WORKDIR is a base directory in which to build packages
- JOBS is the number of cores to compile with
- INSTALLDIR is the installation directory
- BASE_PROFILE is a file from which to install the base environment (e.g. module load commands for HPC)
- OUTPUTDIR is a directory in which package profile files will be installed

#### B. Set Up Environment

If you followed section A to install using scripts provided you should be able to set up your environment by sourcing the profiles created for MOOSE and OpenMC.
You can provide these files as a comma separated list in the next section.

If you have installed your dependencies not using the scripts provided, you are responsible for ensuring your environment is sane:
1. The Makefile for AURORA assumes the following environment variables have been set:
 - `PETSC_DIR`: path to PETSc installation directory.
 - `MOOSE_DIR`: path to MOOSE installation directory.
 - Optionally you may want to set `MOOSE_JOBS` to the number of cores on your system.
2. In addition it is expected that the following MOAB / DAGMC / OpenMC executables can be found in your PATH: `openmc` ; `mbconvert`; `make_watertight`.

Finally, in order to run AURORA, you will need to have set the variable `OPENMC_CROSS_SECTIONS` to point to a cross_sections.xml file, see [here](https://docs.openmc.org/en/stable/usersguide/cross_sections.html#environment-variable) for more details.

#### C. Build
Assuming you have followed sections A and B, you can run the following script to compile AURORA:
```
./scripts/build-aurora.sh -j $JOBS -e $ENVS -o $OUTPUTDIR -d $AURORADIR
```
where
- JOBS is the number of cores to compile with
- ENVS is a comma-separated list of profiles to source the environment (e.g. MOOSE and OpenMC)
- OUTPUTDIR is a directory in which package profile files will be installed
- AURORADIR is the path to the root AURORA directory (default = $PWD)

If everything succeeded there should now be an executable `aurora-opt` in the root aurora directory and all the tests should have passed.

#### D. Run Tests
The tests should have automatically run if you used the provided script in the previous section.

However, if you choose to do development work, you may wish to run the test suite.
Before you do this ensure the `OPENMC_CROSS_SECTIONS` environment variable points at a cross section file which contains KERMA-processed cross sections. For convenience we package a minimal set of cross sections needed for the tests - you'll need to set the `OPENMC_CROSS_SECTIONS` variable:
```
export OPENMC_CROSS_SECTIONS=/PATH-TO-AURORA/aurora/data/endfb71_hdf5/cross_sections.xml
```
See [this discussion](https://openmc.discourse.group/t/nuclear-data-dependent-zero-result-for-heating-local-tally/833) for further details. Now you can run the tests as follows
```
cd aurora && \
./run_unit_tests && \
./run_tests
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
 - `geometry.xml`
 - `dagmc.h5m`

The `dagmc.h5m` file is a surface mesh file, in length units assumed to be cm. This [guide](https://svalinn.github.io/DAGMC/usersguide/trelis_workflow.html) provides details on the workflow to create such a file.

The AURORA application itself requires two input files, and an exodus file. For details on the syntax of these input files, see the following section.

The first input file controls the main driver application (which performs the FEA), and the second input file (referenced by the first) controls the sub-app that calls OpenMC. We recommend you have read [this guide](https://mooseframework.inl.gov/syntax/MultiApps/index.html) on multiapps before proceeding.

Finally, the exodus file should contain a tetrahedral mesh of the geometry of interest. Currently this geometry needs to be the same as that in the dagmc.h5m file, however in future we intend to support the case where the FEA geometry is a subset of the OpenMC geometry. It is possible to use different lengthscales between the exodus and dagmc files, in which case the parameter `length_scale` for the  `MoabUserObject` should be set (e.g. to convert from m in the exodus file into cm in dagmc.h5m this parameter would be 100). Note that although MOOSE generically is unit-agnostic, it is necessary to have consistency with physical material properties. Thus, if you use m in your exodus file, ensure all length-dimensionful material properties are also given values in units of m.

There are two examples provided in the test directory.
The first example performs a simple neutronics + heat conduction simulation.
```
cd test/tests/thermal
../../../aurora-opt -i main.i
```
The second example performs neutronics + heat conduction + thermal expansion.
```
cd test/tests/thermal-mech
../../../aurora-opt -i main_temp_mech.i
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
