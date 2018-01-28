SpectreCoin
===========

This is the official source code repository for [Spectrecoin](https://spectreproject.io/) (XSPEC).

The latest release is [1.3.5](https://github.com/XSPECOfficial/spectre/releases/tag/v1.3.5), released on January 16, 2018.

Building from source
====================

**NOTE** that these instructions are relevant for building from master, which is the latest code in development. It is generally stable but can contain features that have had less testing than released versions. If you want to build a stable version of Spectre, please check out the latest release tag (v1.3.5) before you start building.

Dependencies
------------

We do not currently provide Linux binary packages. To build the SpectreCoin wallet from source, you will need the following dependencies:

 * OpenSSL 1.1
 * libevent
 * libseccomp
 * libcap
 * boost
 * Qt 5 and QtWebKit if you want to build the GUI wallet. Qt is not needed for the console wallet.

Additionally, you'll need a C/C++ compiler and the basic dependencies needed for any kind of development. On most Linux distributions there is a metapackage that installs these. On macOS this means you will need Xcode and the Command Line Tools.

To check all dependencies and install missing ones on **Debian/Ubuntu/Mint/etc**:

    apt install build-essential libssl-dev libevent-dev libseccomp-dev libcap-dev libboost-all-dev pkg-config
    # the following commands should only be run if building the GUI wallet
    apt install qtbase5-dev qttools5-dev-tools libqt5webkit5-dev qtchooser

To check all dependencies and install missing ones on **Arch Linux**:

    pacman -S --needed base-devel openssl libevent libseccomp libcap boost
    pacman -S --needed qt5  # only if building the GUI wallet

To check all dependencies and install missing ones on **macOS** (this uses the [Homebrew](https://brew.sh/) package manager; if you use something else then adjust the commands accordingly):

    brew install autoconf automake libtool pkg-config openssl@1.1 libevent boost gcc
    # the following commands are only needed if building the GUI wallet
    brew tap KDE-mac/homebrew-kde
    brew install qt qt-webkit

Building
--------

To fetch the source code and build the wallet run the following commands:

    git clone --recursive https://github.com/XSPECOfficial/spectre
    cd spectre
    ./autogen.sh
    export QT_SELECT=qt5  # only necessary on Debian/Ubuntu/Mint/etc
    ./configure --enable-gui  # leave out --enable-gui to build only the console wallet
    make -j2  # use a higher number if you have many cores and memory, leave -j2 out if you are on a very low-powered system like Raspberry Pi

The resulting binaries will be in the `src` directory and called `spectre` for the GUI wallet and `spectrecoind` for the console wallet.

Cross-compiling for Windows is supported using MingW64, by passing the appropriate `--host` parameter to `./configure`.
