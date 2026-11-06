![C++](https://img.shields.io/badge/c++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Qt](https://img.shields.io/badge/Qt-%23217346.svg?style=for-the-badge&logo=Qt&logoColor=white)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![DOI](https://zenodo.org/badge/1274359692.svg)](https://doi.org/10.5281/zenodo.24011261)

![EddyFlow](img/logo/app-logo.svg)

# Welcome to EddyFlow

EddyFlow is a powerful open source software application for processing eddy covariance data. It computes fluxes of water vapor (evapotranspiration), carbon dioxide, and other trace gases, and energy with the Eddy Covariance method.

[LI-COR Biosciences](http://www.licor.com) has not published upstream EddyPro updates for several years. EddyFlow is an actively maintained fork of EddyPro 6.2.2 that continues development while gratefully acknowledging LI-COR, ECO2S, and the original EddyPro authors for creating and releasing EddyPro. EddyPro originates from [ECO<sub>2</sub>S](http://gaia.agraria.unitus.it/eco2s), the Eddy COvariance COmmunity Software project, which was developed as part of the Infrastructure for Measurement of the European Carbon Cycle (IMECC-EU) research project. We gratefully acknowledge the [IMECC](http://imecc.ipsl.jussieu.fr/index.html) consortium, the ECO<sub>2</sub>S development team, the [University of Tuscia](https://www.unitus.it) (Italy) and scientists around the world who assisted with development and testing of the original version of this software.

## Overview

EddyFlow is a cross-platform application (Windows, Linux and macOS), which consists of a set of command line programs and a graphical user interface (GUI). It consists of 4 repositories:

- [eddyflow-engine](https://github.com/kebasaa/eddyflow-engine): The core engine, a command line software
- [eddyflow-gui](https://github.com/kebasaa/eddyflow-gui) (current repository): The Qt6 graphical user interface to configure and run the engine
- [eddyflow-documentation](https://github.com/kebasaa/eddyflow-documentation): The documentation of the software and its usage
  It provides the [documentation website](https://kebasaa.github.io/eddyflow-documentation/)
- [eddyflow-build-script](https://github.com/kebasaa/eddyflow-build-script): This builds the software binaries for Windows, Linux and macOS

## Installing EddyFlow

Download EddyFlow from the [releases](https://github.com/kebasaa/eddyflow-gui/releases) page:

- **Windows**: a portable `.7z` archive (x86_64 and arm64). The contained folder is fully portable and does not require installation.
- **Linux**: a `.deb` package for Ubuntu, an `.rpm` package for Fedora, and a `.snap`.
- **macOS**: a `.dmg` disk image.

EddyFlow checks for a new release when it starts, and under Help > Check for Updates. On Windows it downloads the portable archive and replaces its own files, rolling back if anything fails. On macOS it installs the new `.dmg` over the running application when the application folder is writable. On Linux, and wherever it cannot write to its own folder, it opens the release page instead.

## Building EddyFlow from source

The GUI is a Qt6 project, built with qmake from `EddyFlow_win.pro`, `EddyFlow_lin.pro` or `EddyFlow_mac.pro`, and needs [QuaZIP](https://github.com/stachenov/quazip). The release packages are built with the scripts in [eddyflow-build-script](https://github.com/kebasaa/eddyflow-build-script), which also builds the engine and puts both together.

Publishing a release here runs the workflows in `.github/workflows` (`windows-release.yml`, `linux-release.yml` and `macos-release.yml`). They build the packages on GitHub runners, using the eddyflow-engine tag with the same name, and attach them to the release. They need the repository secret `BUILD_SCRIPT_TOKEN`, a token with read-only access to the eddyflow-build-script repository. To rebuild an existing release, run the workflow from the Actions tab and enter the tag.

## Utilities

To successfully run EddyFlow, the program installation folder must contain the following command line utilities under the 'bin' sub-directory:

- 7-zip (Windows only; Linux and macOS use the system's)
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

## What the interface adds

+ Express, Advanced and Metadata Retriever runs, with every setting of the engine in one place
+ Opens EddyPro projects (`.eddypro`) and imports EddyUH projects, without writing anything back to the originals
+ Reads inputs from a Google Drive or Dropbox folder shared with "anyone with the link": a "Remote drive..." button sits beside every "Browse..." button (output folders stay local)
+ Parallel processing across all processor cores, with a progress bar that follows the split
+ Stop, pause and resume reach every process of a run
+ Per-gas settings in each gas's own unit, for projects with any number of gases
+ SMARTFlux package output
+ Offline help
+ Updates itself (see above)

## Data Processing Options in EddyFlow

The engine is described in [eddyflow-engine](https://github.com/kebasaa/eddyflow-engine). Options of the processing steps, as set in the interface:

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
  - Pre-whitening block-bootstrap time lag detection (Vitale et al. 2024), also for slow gases (such as a 1 Hz laser analyser on a 20 Hz sonic), each detected at its own rate
  - Automatic time lag optimization (optionally as a function of RH for H<sub>2</sub>O, with its own classes for each closed-path hygrometer)
  - Maximum covariance with default (circular correlation)
  - Maximum covariance without default
  - Constant
  - None (option to not apply compensation)
  - Conditional lag borrowing: a gas whose lag cannot be told from noise may take the lag of another gas on the same analyser (best-resolved, or a donor chosen per gas)

+ Statistical tests for raw time series data (Vickers and Mahrt, 1997)
  - Spike count/removal (Vickers and Mahrt, 1997; Mauder et al., 2013; or consecutive differences)
  - Amplitude resolution
  - Dropouts
  - Absolute limits (per gas)
  - Skewness and kurtosis
  - Discontinuities
  - Time lags
  - Angle of attack
  - Steadiness of horizontal wind
  - RFlux diagnostics, including the Hartigan dip test (Vitale et al., 2020)
  - Post-flux despiking
  - Individually selectable and customizable

+ Compensation for air density fluctuations
  - Webb et al., 1980 (open path) / Ibrom et al., 2007a (closed path)
  - Use (or convert to) mixing ratio (Burba et al., 2012)
  - Optional off-season uptake correction for LI-7500 (Burba et al., 2008)
  - None (option to not apply compensation)

+ Correction for frequency response (attenuation)
  - Analytic high-pass filtering correction (Moncrieff et al., 2004)
  - Low-pass filtering, select and configure:
    - Moncrieff et al. (1997)
    - Massmann (2000)
    - Horst (1997)
    - Ibrom et al. (2007b)
    - Fratini et al. (2012)
    - Eugster and Senn (1995)
    - Custom transfer function
  - Horst and Lenschow (2009) correction for sensor separation, as an addition to the above
  - Cospectral model for the analytic correction: Moncrieff et al. (1997), Kaimal et al. (1972), Sakai et al. (2001), Su et al. (2004), Moraes et al. (2008) or Kristensen et al. (1997)
  - In-situ methods fall back on Moncrieff et al. (1997) for a gas whose fit fails; the run lists those periods at its end
  - Optionally iterate the correction

+ Quality control tests for fluxes according to Foken et al. (2004)
  - Flagging according to Carbo Europe standard (Mauder and Foken, 2004)
  - Flagging according to Foken (2003)
  - Flagging after Göckede et al. (2004)
  - Flagging according to Vitale et al. (2020)

+ Random uncertainty estimation
  - Finkelstein and Sims (2001)
  - Mann and Lenschow (1994)
  - Mahrt (1998)
  - Billesbach (2011)
  - Lenschow et al. (2000), as applied by Mauder et al.
  - Optionally spectrally corrected

+ Detection limits
  - Flux detection limit (Wienhold et al., 1994)
  - Limit of detection after Langford et al. (2015), a switch of its own that works beside any random uncertainty method

+ Flux footprint estimation
  - Kljun et al. (2004)
  - Kormann and Meixner (2001)
  - Hsieh et al. (2000)
  
+ ET & NEE Partitioning
  - Conditional Eddy Covariance (Zahn et al. 2022), over any number of
    CO2/H2O channel pairings, and extending to any further species measured
    alongside them - carbonyl sulfide included. Beyond Zahn et al. these parts are experimental:
    carbonyl sulfide as the octant gas, one flux partitioned several ways, and flagging of noisy octants

+ Other options applied in both Express and/or Advanced Mode include:
  - Sonic temperature correction for humidity following van Dijk et al. (2004)
  - Spectroscopic correction for LI-7700 following McDermitt et al. (2011), and removal of the spectroscopic effect of water vapour from closed-path data
  - Angle of attack corrections for Gill anemometers following Nakai et al. (2006)
  - Angle of attack corrections for Gill anemometers following Nakai and Shimoyama (2012)
  - Head correction for Metek USA-1 anemometers, and inclinometer tilt correction
  - Invalid wind sectors that change over time, and linear calibration per channel
  - Inclusion of biomet data for improved flux computation/correction

+ Data it can read
  - Any number of gases per project, each with its own unit, limits and moisture reference
  - LI-COR GHG files whose acquisition frequency changes part way; spectra are assessed separately at each rate
  - Instruments that state their own acquisition frequency, sampled slower than the file

+ Available outputs
  - Full (rich) output with fluxes, quality flags and much more (standard format
    or available results only)
  - FLUXNET output (complying latest FLUXNET format definition)
  - Raw data statistics
  - Full length spectra and co-spectra
  - Binned spectra and co-spectra
  - Binned ogives
  - Ensemble averaged spectra
  - Ensemble averaged cospectra, fitted models and ideal (Kaimal) cospectra
  - Details of steady state and turbulence tests
  - Raw data time series after each statistical tests/correction
  - Averaged biomet data
  - A column legend for the output files, and a log of every run

## Corrections to EddyPro 6.2.2

EddyFlow also corrects errors that EddyPro 6.2.2 contained. They are listed in the [engine README](https://github.com/kebasaa/eddyflow-engine#corrections-to-eddypro-622), and every change is in the [CHANGELOG](CHANGELOG).

## EddyFlow Trademark and Logo Policy

In order to help users who want to cite EddyFlow on posters or publications, we provide the [EddyFlow logo as vector graphic](img/logo/app-logo.svg) for the proper use of the EddyFlow wordmark and logo.

## Want to Know More?

More information is available at the help website [EddyFlow help](https://kebasaa.github.io/eddyflow-documentation/).

See also the [CHANGELOG](CHANGELOG).


## How to Cite

Muller, J.D. (2026). *eddyflow-gui: EddyFlow graphical user interface*. DOI: https://doi.org/10.5281/zenodo.24011261

## License

This software is distributed under the [GNU General Public License (GPL) v3.0](LICENSE). Any modification of the code in this repository may only be released under the same license, and with attribution of authorship of the original code (i.e., citation above).
