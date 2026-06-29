![C++](https://img.shields.io/badge/c++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Qt](https://img.shields.io/badge/Qt-%23217346.svg?style=for-the-badge&logo=Qt&logoColor=white)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

![EddyFlow](img/logo/app-logo.svg)

# Welcome to EddyFlow

EddyFlow&reg; is a powerful open source software application for processing eddy covariance data. It computes fluxes of water vapor (evapotranspiration), carbon dioxide, and other trace gases, and energy with the Eddy Covariance method.

[LI-COR Biosciences](http://www.licor.com) has not published upstream EddyFlow updates for several years. This repository is an actively maintained fork of EddyPro 6.2.2 that continues development while gratefully acknowledging LI-COR, ECO2S, and the original EddyFlow authors for creating and releasing EddyFlow.eddypro originates from [ECO<sub>2</sub>S](http://gaia.agraria.unitus.it/eco2s), the Eddy COvariance COmmunity Software project, which was developed as part of the Infrastructure for Measurement of the European Carbon Cycle (IMECC-EU) research project. We gratefully acknowledge the [IMECC](http://imecc.ipsl.jussieu.fr/index.html) consortium, the ECO<sub>2</sub>S development team, the [University of Tuscia](https://www.unitus.it) (Italy) and scientists around the world who assisted with development and testing of the original version of this software.

## Overview

EddyFlow consists of 4 repositories:

- [eddyflow-engine](https://github.com/kebasaa/eddyflow-engine): The core engine, a command line software
- [eddyflow-gui](https://github.com/kebasaa/eddyflow-gui) (current repository): The graphical user interface to configure and run the engine
- [eddyflow-documentation](https://github.com/kebasaa/eddyflow-documentation): The documentation of the software and its usage
- [eddyflow-build-script](https://github.com/kebasaa/eddyflow-build-script): This builds the software binaries (currently Windows only)

## License

This fork of EddyPro 6.2.2 is released with and will retain the
[GNU General Public License (GPL) v3.0](LICENSE).

## Source Code Repository

EddyFlow is a fully cross-platform application, which consists of a set of command line programs and a graphical user interface (GUI).

The source code is developed using two independent Git repositories, namely:

  - [EddyFlow-engine](https://github.com/kebasaa/EddyFlow-engine)
  - [EddyFlow-gui](https://github.com/kebasaa/EddyFlow-gui)

## Installing EddyFlow

You can download EddyFlow from the releases .7z file, the contained folder is fully portable and does not require installation.

## Building EddyFlow from source

To build EddyFlow, use the Powershell script on [eddyflow-build-script](https://github.com/kebasaa/eddyflow-build-script). This builds the software binaries (currently Windows only).

### Engine

To compile the Engine only, you can use [gfortran](https://gcc.gnu.org/wiki/GFortran) (The GNU Fortran compiler) and run:

    $ cd prj
    $ make rp
    $ make fcc

<!-- ### GUI

Source code compilation instructions for the GUI are undergoing a revision. They will posted as soon the update is completed.

To compile the GUI:

1. install the [Qt framework](https://www.qt.io/developers/)
2. install [git](http://git-scm.com/)
3. build the Qt `EddyFlow.pro` project file using custom build scripts available
   under `source/scripts/build` or using QtCreator

In both cases, the build configuration will be shadowed or out-of-tree, i.e. created in a dedicated
directory outside the source tree.

#### Build the GUI using provided scripts

##### On Windows

Launch git-bash and enter the following commands, where `EddyFlow-source-dir` is
the directory where the source code is:

    1. $ cd EddyFlow-source-dir/source/scripts/build/
    2. $ ./win-build-EddyFlow.sh [debug|release]

##### On Mac

In a terminal enter the following commands, where `EddyFlow-source-dir` is the
directory where the source code is:

    1. $ cd EddyFlow-source-dir/source/scripts/build/
    2. $ ./mac-build-EddyFlow.sh [debug|release]

#### Build the GUI using QtCreator (on Windows or Mac)

    1. Open 'source\EddyFlow.pro'
    2. Open 'source\libs\quazip\quazip\EddyFlow.pro'
    3. In the 'EddyFlow.pro' project settings:
        3.1 set the build directory to '..\build\build-EddyFlow-6.1.0-qt-5.4.2-mingw-4.9.1-x86_64'
            for both debug and release targets
        3.2 check all the listed libs as dependencies
    4. In the 'quazip.pro' project settings:
        4.1 set the build directory to '..\..\..\..\build\libs\build-quazip-0.7.1-qt-5.4.2-mingw-4.9.1-x86_64'
            for both debug and release targets
    5. In the 'EddyFlow.pro' project, build both targets -->

## Utilities

To successfully run EddyFlow, the program installation folder must contain the following command line utilities under the 'bin' sub-directory:

- 7-zip
- Quazip
- Zlib

### 7-zip

[7-Zip](http://www.7-zip.org/) is a file archiver.

The console application consists of two files:
- 7z.dll
- 7z.exe

License: [LGPL](https://www.7-zip.org/license.txt).

## Using EddyFlow sample data

You can run EddyFlow using sample data files available at the [eddyflow-build-script](https://github.com/kebasaa/eddyflow-build-script) repository.

## Data Processing Options in EddyFlow

+ Axis rotation for sonic anemometer tilt correction
  - Double rotation
  - Triple rotation
  - Sector-wise planar fit (Wilczak et al., 2001)
  - Sector-wise planar fit with no velocity bias (van Dijk et al., 2004)

+ Detrending of raw time series
  - Block averaging
  - Linear detrending
  - Running mean
  - Exponential running mean

+ Compensation of time lag between sonic anemometer and gas analyzer measurements
  - Pre-whitening block-bootstrap time lag detection (Vitale et al. 2024)
  - Automatic time lag optimization (optionally as a function of RH for H<sub>2</sub>O)
  - Maximum covariance with default (circular correlation)
  - Maximum covariance without default
  - Constant
  - None (option to not apply compensation)

+ Statistical tests for raw time series data (Vickers and Mahrt, 1997)
  - Spike count/removal (Mauder et al., 2013)
  - Amplitude resolution
  - Dropouts
  - Absolute limits
  - Skewness and kurtosis
  - Discontinuities
  - Time lags
  - Angle of attack
  - Steadiness of horizontal wind
  - Individually selectable and customizable

+ Compensation for air density fluctuations
  - Webb et al., 1980 (open path) / Ibrom et al., 2007a (closed path)
  - Use (or convert to) mixing ratio (Burba et al., 2012)
  - Optional off-season upatake correction for LI-7500 (Burba et al., 2008)
  - None (option to not apply compensation)

+ Correction for frequency response (attenuation)
  - Analytic high-pass filtering correction (Moncrieff et al., 2004)
  - Low-pass filtering, select and configure:
    - Moncrieff et al. (1997)
    - Massmann (2000)
    - Horst (1997)
    - Ibrom et al. (2007b)
    - Horst and Lenschow (2009)
    - Fratini et. al. (2012)

+ Quality control tests for fluxes according to Foken et al. (2004)
  - Flagging according to Carbo Europe standard (Mauder and Foken, 2004)
  - Flagging according to Foken (2003)
  - Flagging after Göckede et al. (2004)

+ Random uncertainty estimation
  - Mann and Lenschow (1994)
  - Finkelstein and Sims (2001)

+ Flux footprint estimation
  - Kljun et al. (2004)
  - Kormann and Meixner (2001)
  - Hsieh et al. (2000)
  
+ ET & NEE Partitioning
  - Conditional Eddy Covariance (Zahn et al. 2022)

+ Other options applied in both Express and/or Advanced Mode include:
  - Sonic temperature correction for humidity following van Dijk et al. (2004)
  - Spectroscopic correction for LI-7700 following McDermitt et al. (2011)
  - Angle of attack corrections for Gill anemometers following Nakai et al. (2006)
  - Angle of attack corrections for Gill anemometers following Nakai and Shimoyama (2012)
  - Inclusion of biomet data for improved flux computation/correction

+ Available outputs
  - Full (rich) output with fluxes, quality flags and much more (standard format
    or available results only)
  - FLUXNET output (complying latest FLUXNET format definition)
  - Ameriflux format
  - GHG Europe format
  - Raw data statistics
  - Full length spectra and co-spectra
  - Binned spectra and co-spectra
  - Binned ogives
  - Ensemble averaged spectra
  - Ensemble averaged cospectra, fitted models and ideal (Kaimal) cospectra
  - Details of steady state and turbulence tests
  - Raw data time series after each statistical tests/correction
  - Averaged biomet data

## EddyFlow Trademark and Logo Policy

In order to help users who want to cite EddyFlow on posters or publications, we provide the [EddyFlow logo as vector graphic](img/logo/app-logo.svg) for the proper use of the EddyFlow wordmark and logo.

## Want to Know More?

More information is available at the help website [EddyFlow help](https://kebasaa.github.io/eddyflow-documentation/).

See also the [CHANGELOG](CHANGELOG).

