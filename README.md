SpectreCoin
===========

This is the official source code repository for [Spectrecoin](https://spectreproject.io/) (XSPEC).

The latest release is [1.3.3](https://github.com/spectrecoin/spectre/releases/tag/v1.3.3), released on September 12, 2017.

Building on Linux
-----------------

**To build a stable wallet from source, please download the source code of the latest release from the [releases page](https://github.com/spectrecoin/spectre/releases). Building from the master branch (the development code for the next release) should only be done if you plan to work on the code and understand the risks.**

We do not currently provide Linux binary packages. To build the SpectreCoin wallet from source, you will need the following dependencies:

 * OpenSSL 1.0
 * libevent
 * libseccomp
 * libcap
 * boost
 * Qt 4 and QtWebKit if you want to build the GUI wallet. Qt is not needed for the console wallet.

Additionally, you'll need a C/C++ compiler and the basic dependencies needed for any kind of development. On most Linux distributions there is a metapackage that installs these.

To check all dependencies and install missing ones on **Debian or Ubuntu**:

    $ apt install build-essential libssl1.0-dev libevent-dev libseccomp-dev libcap-dev libboost-all-dev
    $ apt install libqt4-dev libqtwebkit-dev  # only if building the GUI wallet

To check all dependencies and install missing ones on **Arch Linux**:

    $ pacman -S --needed base-devel openssl-1.0 libevent libseccomp libcap boost
    $ pacman -S --needed qt4  # only if building the GUI wallet
    $ # you will also need qtwebkit, which is in AUR. this example uses the pacaur helper:
    $ pacaur -S --needed qtwebkit-bin  # only if building the GUI wallet

On all platforms, to build the wallet run the following commands:

    $ ./autogen.sh
    $ ./configure --enable-gui  # leave out --enable-gui to build only the console wallet
    $ make -j2  # use a higher number if you have many cores and memory

If your distribution provides both OpenSSL 1.0 and OpenSSL 1.1, you may need to use the `PKG_CONFIG_PATH` environment variable to point `configure` to the directory that contains the `openssl.pc` file for OpenSSL 1.0. For example, on Arch Linux it's necessary to do this:

    $ PKG_CONFIG_PATH=/usr/lib/openssl-1.0/pkgconfig ./configure --enable-gui

Please note that building with `clang` is not currently supported, due to a limitation in Berkeley DB. If `clang` is the default compiler on your system please use the `CC` and `CXX` environment variables when calling `configure` to select `gcc` instead like this:

    $ CC=gcc CXX=g++ ./configure --enable-gui

The resulting binaries will be in the `src` directory and called `spectre` for the GUI wallet and `spectrecoind` for the console wallet.
