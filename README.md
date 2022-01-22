# pci-edu

## What even is this?

This is a proof of concept that shows how easy it *could* be to interact with custom firmware on a PCIe device.

TODO: insert graphic from https://asciiflow.com/legacy/

The main tool in use here is [Apache Thrift](https://github.com/apache/thrift) which is an [RPC framework](https://en.wikipedia.org/wiki/Remote_procedure_call) and code generator. Thrift allows the application programmer to develop and link code that works equally well over a `/dev` node as over a TCP socket. In Linux, both are simple file descriptors.

# Instructions

1. Install build dependencies (Ubuntu Impish Indri)
    ```bash
    sudo apt-get update
    # Install common build tools
    sudo apt install -y build-essential
    # Install Qemu build dependencies
    # https://wiki.qemu.org/Hosts/Linux
    sudo apt install -y libsdl1.2-dev libaio-dev libbluetooth-dev libbrlapi-dev libbz2-dev libcap-dev libcap-ng-dev libcurl4-gnutls-dev libjpeg8-dev libncurses5-dev libnuma-dev librbd-dev librdmacm-dev libsasl2-dev libseccomp-dev libsnappy-dev libssh2-1-dev libvde-dev libvdeplug-dev libxen-dev liblzo2-dev valgrind xfslibs-dev libnfs-dev libiscsi-dev libusb-dev libusbredirparser-dev libgtk-3-dev ninja-build libcurl-dev
    # Install Thrift build dependencies
    # https://thrift.apache.org/docs/install/debian.html
    sudo apt install -y automake bison flex g++ git libboost-all-dev libevent-dev libssl-dev libtool make pkg-config
    ```
1. Fetch PCI Edu sources
    ```bash
    # switch to some workspace folder
    export WS=${PWD}
    git clone https://github.com/cfriedt/pci-edu
    ```
1. Patch & Build Qemu
    ```bash
    git clone https://github.com/qemu/qemu
    cd qemu
    patch -p1 < ${WS}/pci-edu/0001-qemu-hw-misc-edu-fix-2-off-by-one-errors.patch
    patch -p1 < ${WS}/pci-edu/0002-qemu-hw-misc-edu-perform-dma-over-socket.patch
    mkdir build
    cd build
    ../configure
    ninja
    sudo ninja install
    cd ${WS}
    ```
1. Build & Install Thrift
    ```bash
    git clone https://github.com/apache/thrift
    cd thrift
    autoreconf -vfi
    ./configure
    make -j
    sudo make -j install
    cd ${WS}
    ```
1. Build App & Module
    ```bash
    cd ${WS}/pci-edu
    make -j
    ```
1. Run the socket-based test
    ```shell
    make app-check
    ```
1. Check expected output:
    ```bash
    Starting the server...
    ping()
    echo('Hello, world!')
    counter() => 0
    counter() => 1
    counter() => 2
    counter() => 3
    counter() => 4
    ```
1. Run the pcie-based test
    ```bash
    make module-check
    ```
1. Check expected output:
    ```bash
    Starting the server...
    ping()
    echo('Hello, world!')
    counter() => 0
    counter() => 1
    counter() => 2
    counter() => 3
    counter() => 4
    ```

# Areas to Improve

## Async Messaging
Async Messaging is sort of synonymous with non-blocking I/O. Non-blocking I/O might Just Work™, but the `poll()` and `ioctl()`  calls would probably need some attention. If only ever performing blocking I/O, then this is not necessary.

## Reduce Copying
Currently, the entire Thrift message is copied to an internal DMA buffer. This was simple enough to do in order to make Thrift work OOTB but generally copying is bad. Additionally, the edu DMA buffer is fixed in size at 4096 bytes. It would be more ideal if we could allocate variably-sized DMAable regions from the device itself, possibly persist those over time, and have some system to indicate when one (or several) buffer(s) can be freed. There are also some pretty advanced techniques that could be employed to DMA large buffers directly. It would be useful to support scatter-gather APIs as well.

Most of these changes would require modifications to the Thrift code generator.
