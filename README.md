*Copyright (C) 2022, Axis Communications AB, Lund, Sweden. All Rights Reserved.*

<!-- omit in toc -->
# BETA - ACAP runtime

[![Build docker-image application](https://github.com/AxisCommunications/acap-runtime/actions/workflows/docker-image.yml/badge.svg)](https://github.com/AxisCommunications/acap-runtime/actions/workflows/docker-image.yml)

- [ ] TODO: Once all TODOs are ticked off, remove them before commiting this file

- [ ] TODO: Link to https://axiscommunications.github.io/acap-documentation/docs/api/computer-vision-sdk-apis.html#beta---acap-runtime

ACAP runtime is a network protocol based service using [gRPC](https://grpc.io/) and Unix Domain Socket (UDS) for access. This makes the service available to clients written in different languages on the same device.

<!-- omit in toc -->
## Table of contents

- [Overview](#overview)
  - [Requirements](#requirements)
  - [APIs](#apis)
  - [Access point](#access-point)
  - [TLS](#tls)
- [Installation and usage](#installation-and-usage)
  - [As an ACAP application](#as-an-acap-application)
  - [As containerized version](#as-containerized-version)
  - [Settings](#settings)
- [Building ACAP runtime](#building-acap-runtime)
  - [Building as an ACAP application](#building-as-an-acap-application)
  - [Building as a containerized version](#building-as-a-containerized-version)
  - [In a VSCode Devcontainer](#in-a-vscode-devcontainer)
- [Test suite](#test-suite)
- [Contributing](#contributing)
- [License](#license)

## Overview

- [ ] TODO: Describe the benefits of gRPC support for ACAP

ACAP runtime can either be used as an installed ACAP application or as a containerized version.

### Requirements

- [ ] TODO: Is there a template for this?
- [ ] TODO: Check if min versions are correct

The following requirements need to be met.

- Axis device:
  - ACAP runtime as an ACAP application can be installed on any device that the ACAP Native SDK suppots. See [Axis devices & compatibility](https://axiscommunications.github.io/acap-documentation/docs/axis-devices-and-compatibility#sdk-and-device-compatibility) for more information.
  - ACAP runtimes containerized version also requires [Docker ACAP](https://github.com/AxisCommunications/docker-acap) to be installed and running.
  - Certificate files if [TLS](#tls) is used.

- Computer:
  - Either [Docker Desktop](https://docs.docker.com/desktop/) version 4.11.1 or higher, or [Docker Engine](https://docs.docker.com/engine/) version 20.10.17 or higher, with BuildKit enabled.

### APIs

The ACAP runtime service includes the following APIs:

- Inference API - An implementation of [Tensorflow Serving](https://github.com/tensorflow/serving).
- Parameter API - Provides gRPC read access to the parameters of an Axis device that otherwise would be read out via [VAPIX](https://www.axis.com/vapix-library/subjects/t10175981/section/t10036014/display?section=t10036014-t10036014). There are usage examples available for the parameter API in [Python](https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/main/parameter-api-python) and [C++](https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/main/parameter-api-cpp).

### Access point

When started (with default settings) the ACAP runtime service provides an Unix Domain Socket (UDS) access point:

```sh
acap-runtime.sock
```

### TLS

- [ ] TODO: Describe or link to how to generate certificates
- [ ] TODO: # Generate TSL/SSL test certificate
RUN openssl req -x509 -batch -subj '/CN=localhost' -days 10000 -newkey rsa:4096 -nodes -out server.pem -keyout server.key

This service can be run either unsecured or in TLS mode. TLS mode provides additional security and encryption on the gRPC channel. There is a "Use TLS" dropdown in the web interface to switch between the two different modes. Note that the service has to be restarted every time TLS is activated or deactivated. TLS requires certificate and key file to work, which are listed below. For more information on how to generate these files, please see the [Parameter API](https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/master/parameter-api) example.

```sh
/usr/local/packages/acapruntime/server.pem
/usr/local/packages/acapruntime/server.key
```
  
## Installation and usage

### As an ACAP application

- [ ] TODO: What is the correct tag for the image(s)?

The recommended way to install and run ACAP runtime is to use the pre-built
[docker hub](https://hub.docker.com/r/axisecp/acap-runtime) image. For information on how to build locally see [Building ACAP runtime](#building-acap-runtime).

When the docker image is run, this installs ACAP runtime as an ACAP application on the device, where it can be controlled in the device GUI **Apps** tab.

To install ACAP runtime on a device the following command can be run from the host computer:

```sh
# Install the latest prebuilt image
docker run --rm axisecp/acap-runtime:latest-<ARCH> <device ip> <device password> install
```

Where `<ARCH>` is either `armv7hf` or `aarch64` depending on device architecture, `<device ip>` is the ip address of the device and `<device password>` is the password for the root user.

The application can then be started either in the device GUI **Apps** tab or by running:

```sh
docker run --rm axisecp/acap-runtime:latest-<ARCH> <device ip> <device password> start
```

The application log can be found by clicking on the "***App log***" in the application drop down menu in the device GUI, or directly at:

```sh
http://<device_ip>/axis-cgi/admin/systemlog.cgi?appname=acapruntime
```

The application can be stopped and uninstalled by using the device GUI, or by running:

```sh
docker run --rm axisecp/acap-runtime:latest-<ARCH> <device ip> <device password> stop
docker run --rm axisecp/acap-runtime:latest-<ARCH> <device ip> <device password> remove
```
<!-- omit in toc -->
#### Example usages of ACAP application version

The following examples use the Parameter API with ACAP runtime as an ACAP application:

- [parameter-api-cpp](https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/main/parameter-api-cpp)
- [parameter-api-python](https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/main/parameter-api-python)

### As containerized version

Whereas the standard ACAP runtime docker image will install the service as an ACAP application, the containerzied version allows to run it in a container on the device. This requires that [Docker ACAP](https://github.com/AxisCommunications/docker-acap) is installed and running on the device.

Pre-built containerized images are available on [axisecp/acap-runtime](https://hub.docker.com/r/axisecp/acap-runtime) with tags on the form \<version>-\<ARCH>-containerized.
To include the containerzied acap runtime server in a project it can e.g. be included in a docker-compose.yml file:

```sh
version: '3.3'
services:
    acap-runtime-server:
        image: axisecp/acap-runtime:latest-armv7hf-containerized
    entrypoint: ["/opt/app/acap_runtime/acapruntime", "-o", "-j", "4"]
    acap-runtime-client:
        image: <client app image>
        environment:
            - INFERENCE_HOST=unix:///tmp/acap-runtime.sock
            - INFERENCE_PORT=0
    <any other apps>
```

<!-- omit in toc -->
#### Example usages of containerized version

The following examples use the Inference API with ACAP runtime in the containerized version:

- [minimal-ml-inference](https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/main/minimal-ml-inference)
- [object-detector-cpp](https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/main/object-detector-cpp)

### Settings

When starting the ACAP runtime service from command line, as is done with the containerized version, it accepts the following settings:

```sh
-v              Verbose, enbale extended logging,
-o              Do not read settings from device parameters. See note1,
-a <ip address> IP address of gRPC server, default 0.0.0.0. See note2,
-p <ip port>    IP port of gRPC server, default 0. See note2,
-t <seconds>    Runtime in seconds (used for test),
-c <file name>  Certificate file for TLS authentication. See note3,
-k <file name>  Private key file for TLS authentication. See note3,
-j <chip id>    Chip id used by Inference API server. See note4,
-m <file name>  Larod model file used by Inference API server
```

There are also device parameters that can be controlled from the application drop down menu in the device GUI when ACAP runtime is installed as an ACAP application.

```sh
Verbose     Enable extended logging, default No,
IpPort      Ip port of gRPC server. See note2,
Use TLS     Enable SSL/TLS, default No. See note3,
ChipId      Chip id used by Inference API server. See note4
```

- note1 - Setting the *-o* flag makes sure that the command line settings are not overwritten by the device parameters
- note2 - The gRPC server can be set up with either a unix-socket (default) or a network socket. To set up as network socket the ip port should be set to a non 0 value. The ip address is only used when setup as a network socket.
- note3 - To use TLS a certificate file and a corresponding private key file must be supplied. If either is ommited, or if the device setting Use TLS is set to *No*, TLS is not used.
- note4 - When using the Inference API the chip Id corresponding to the devie must be given. See the table below for valid values. If the value is set to 0 (LAROD_CHIP_INVALID) the Inferece API server will not be started.
  
  | Chip Id | Name                          | Description                                |
  |---------|-------------------------------|--------------------------------------------|
  |0        | LAROD_CHIP_INVALID            | Set to not start Inference API server      |
  |4        | LAROD_CHIP_TPU                | Use for device with *armv7hf* architecture |
  |12       | LAROD_CHIP_TFLITE_ARTPEC8DLPU | Use for device with *aarch64* architecture |

## Building ACAP runtime

The easiest way to build ACAP runtime is to use one of the Dockerfiles provided in this repo as described below.

### Building as an ACAP application

The repo provides two Dockerfiles, Dockerfile.armv7hf and Dockerfile.aarch64. Depending on which architecture your device have, either of them can be used to build the ACAP runtime application:

```sh
# Build ACAP runtime image
docker build . -f Dockerfile.<ARCH> --tag acap-runtime:<ARCH>
```

where `<ARCH>` is either `armv7hf` or `aarch64`.
The build is based on a [axisecp/acap-native-sdk](https://hub.docker.com/repository/docker/axisecp/acap-native-sdk). To base it on a different version than what is on main branch you can provide the build arguments `VERSION` and `UBUNTU_VERSION` to select a specific tag of the acap-native-sdk image. E.g. to use [axisecp/acap-native-sdk:1.4_beta1-armv7hf-ubuntu22.04](https://hub.docker.com/layers/axisecp/acap-native-sdk/1.4_beta1-armv7hf-ubuntu22.04/images/sha256-07ed766f7a68033a2717b1334c8fdee29b1a55386b37d67924e5401c91ed9ecd?context=repo):

```sh
docker build . -f Dockerfile.<ARCH> --tag acap-runtime:<ARCH> --build-arg VERSION=1.4beta1 --build-arg UBUNTU_VERSION=22.04
```

### Building as a containerized version

To build the containerized version, use either Dockerfile.armv7h.containerized or Dockerfile.armv7h.containerized:

```sh
# Build acap-runtime containerized version
docker build . -f Dockerfile<ARCH>.containerized --tag acap-runtime:<ARCH>-containerized
```

This pulls the pre-built [axisecp/acap-runtime](https://hub.docker.com/repository/docker/axisecp/acap-runtime) image with tag \<`BUILDVERSION`>-\<`ARCH`>. To use your own locally build acap-runtime image either re-tag it or update the Dockerfile to match. Also note that the base is an Ubuntu image and that that version (build argument `RUNTIME_UBUNTU_VERSION`) must match the version that the acap-runtime image (i.e. the acap-native-sdk image) is based on.

### In a VSCode Devcontainer

- [ ] TODO: Describe if we want to include this as well

## Test suite

The repo contains a test suite project to verify that ACAP runtime works as expected on a supported device.

To install the latest prebuilt test suite image on a device run:

```sh
# Install the latest prebuilt image
docker run --rm axisecp/acap-runtime:latest-<ARCH>-test <device ip> <rootpasswd> install
```

The application can be started, stopped eventually uninstalled in the **Apps** tab in the device GUI or by running:

```sh
docker run --rm axisecp/acap-runtime:latest-<ARCH>-test <device ip> <rootpasswd> start|stop|remove
```

To see the test run output check the application log either by clicking on the **App log** link in the device GUI, or directly at:

```sh
http://<axis_device_ip>/axis-cgi/admin/systemlog.cgi?appname=acapruntimetest
```

If the tests pass the log should end with \[  PASSED  ]. If any test fails, it will be listed.

The test suite can be built locally as well, so that any local changes to ACAP runtime are also tested. Build it by running:

```sh
# Build ACAP runtime test suite image
docker build . -f Dockerfile.<ARCH> --tag acap-runtime:<ARCH>-test --build-arg TEST=yes
```

where `<ARCH>` is either `armv7hf` or `aarch64`.

## Contributing

Take a look at the [CONTRIBUTING.md](CONTRIBUTING.md) file.

## License

[Apache 2.0](LICENSE)
