*Copyright (C) 2022, Axis Communications AB, Lund, Sweden. All Rights Reserved.*

<!-- omit in toc -->
# ACAP Runtime

[![Lint codebase](https://github.com/AxisCommunications/acap-runtime/actions/workflows/lint.yml/badge.svg)](https://github.com/AxisCommunications/acap-runtime/actions/workflows/lint.yml)

ACAP Runtime is a network protocol based service, using [gRPC][gRPC].
This makes the service available to clients written in different languages on
the same device. ACAP Runtime is also described in the [ACAP documentation][acap-documentation-acap-runtime].

If you are new to the world of ACAPs take a moment to check out
[What is ACAP?][acap-documentation]

<!-- omit in toc -->
## Table of contents

- [Overview](#overview)
  - [Requirements](#requirements)
  - [APIs](#apis)
- [Installation and usage](#installation-and-usage)
  - [Installation](#installation)
    - [Native ACAP application](#native-acap-application)
    - [Containerized version](#containerized-version)
  - [Configuration](#configuration)
    - [Native ACAP application](#native-acap-application-1)
    - [Containerized version](#containerized-version-1)
    - [Chip id](#chip-id)
    - [TLS](#tls)
    - [gRPC socket](#grpc-socket)
  - [Examples](#examples)
- [Building ACAP Runtime](#building-acap-runtime)
  - [Native ACAP application](#native-acap-application-2)
  - [Containerized version](#containerized-version-2)
- [Test suite](#test-suite)
- [Contributing](#contributing)
- [License](#license)

## Overview

ACAP Runtime provides a network protocol based service, using gRPC to
expose a number of [APIs](#apis). Once started, ACAP Runtime runs a gRPC server
that can be accessed by a client application, written in any gRPC compatible language.
For further information on the gRPC protocol and how to write gRPC clients see
[https://grpc.io/][gRPC].

![ACAP Runtime Service with gRPC client](assets/gRPC.png)

ACAP Runtime can be installed on a device as a standard native ACAP application.
Additionally it is available in a containerized version, i.e. it can run as a
container on the device.
The former case is mainly suitable for use together with the
[ACAP Native SDK][acap-documentation-native], whereas the latter is aimed to be
used together with the [ACAP Computer Vision SDK][acap-documentation-cv] and
requires that [Docker ACAP][docker-acap] is installed on the device. The
containerized version provides more configuration options. This is further
described in the respective Configuration sub sections in the
[Installation and usage](#installation-and-usage).

> **Note**
>
> The ACAP Runtime service can run with TLS authentication or without.
> Be aware that running without TLS authentication is extremely insecure and we
strongly recommend against this.
> See [TLS](#tls) for information on how to generate certificates for TLS
authentication when using ACAP Runtime.

### Requirements

The following requirements need to be met.

- Axis device:
  - ACAP Runtime as an ACAP application can be installed on any device that the
  ACAP Native SDK supports. See [Axis devices & compatibility][devices] for
  more information.
  - ACAP Runtime's containerized version also requires [Docker ACAP][docker-acap]
  to be installed and running.
  - AXIS OS version 10.7 or higher.
  - Certificate files if [TLS](#tls) is used.

- Computer:
  - Either [Docker Desktop][dockerDesktop] version 4.11.1 or higher, or
  [Docker Engine][dockerEngine] version 20.10.17 or higher, with BuildKit enabled.

### APIs

The ACAP Runtime service provides the following APIs:

- Machine learning API - An implementation of [Tensorflow Serving][tensorflow]. There
  are usage examples available for the Machine learning API written in
  [Python][minimal-ml-inference] and [C++][object-detector-cpp].
- Parameter API - Provides gRPC read access to the parameters of an Axis device.
  There are usage examples available for the Parameter API written in
  [Python][parameter-api-python] and [C++][paramter-api-cpp].
- Video capture API - Enables capture of images from a camera. No usage examples
  for this API exist yet.

## Installation and usage

### Installation

Both variants of ACAP Runtime, the native ACAP application and the containerized
version, are available as pre-built images on [Docker Hub][docker-hub-acap-runtime].
These images are the recommended way to install and use ACAP Runtime.

#### Native ACAP application

To install use any image from [axisecp/acap-runtime][docker-hub-acap-runtime] with
a tag on the form `<version>-<ARCH>`, where `<version>` is the acap-runtime release
version and `<ARCH>` is either `armv7hf` or `aarch64` depending on device architecture.
E.g. `1.1.2-armv7hf`.
Running the image installs ACAP Runtime as an ACAP application on the device,
where it can be controlled in the device GUI **Apps** tab.

```sh
# Install a prebuilt image
docker run --rm axisecp/acap-runtime:<version>-<ARCH> <device IP> <device password> install
```

Where `<device IP>` is the IP address of the device and `<device password>` is the password for the root user.

The application can then be started either in the device GUI **Apps** tab or by running:

```sh
docker run --rm axisecp/acap-runtime:<version>-<ARCH> <device IP> <device password> start
```

The application log can be found by clicking on the **App log** in the
application drop down menu in the device GUI, or directly at:

```sh
http://<device IP>/axis-cgi/admin/systemlog.cgi?appname=acapruntime
```

The application can be stopped and uninstalled by using the device GUI, or by running:

```sh
docker run --rm axisecp/acap-runtime:<version>-<ARCH> <device IP> <device password> stop
docker run --rm axisecp/acap-runtime:<version>-<ARCH> <device IP> <device password> remove
```

#### Containerized version

Whereas the standard ACAP Runtime Docker image will install the service as an ACAP
application, the containerized version allows to run it in a container on the device.
This requires that [Docker ACAP][docker-acap] is installed and running on the device.

Pre-built containerized images are available on
[axisecp/acap-runtime][docker-hub-acap-runtime] with tags on the form
`<version>-<ARCH>-containerized`.
To include the containerized ACAP Runtime server in a project, add the image in
the projects `docker-compose.yml` file. The following is an illustrative
example of how the service can be set up with docker-compose. Here we use the
image for `armv7hf`architecture. For a complete description
see one of the working project [examples](#examples).

```yml
version: '3.3'
services:
    acap-runtime-server:
      image: axisecp/acap-runtime:1.1.2-armv7hf-containerized
      entrypoint: ["/opt/app/acap_runtime/acapruntime", "-o", "-j", "4"]

    acap-runtime-client:
        image: <client app image>
        environment:
            - INFERENCE_HOST=unix:///tmp/acap-runtime.sock
            - INFERENCE_PORT=0
    <any other apps>
```

### Configuration

<!-- markdownlint-disable MD024 -->
#### Native ACAP application
<!-- markdownlint-enable MD024 -->

To change the configuration of the ACAP Runtime service, use the settings in the
application drop down menu in the device GUI. Note that the application needs to
be restarted for any changes to take effect.

The available settings are:

```text
Verbose     Enable extended logging, default 'No',
IpPort      IP port of gRPC server. See note1,
Use TLS     Enable SSL/TLS, default 'Yes'. See note2,
ChipId      Chip id used by Machine learning API service. See note3.
```

Notes.

**(1)** The gRPC server can be set up with either a unix-socket (default) or a
network socket. To set up as network socket IpPort should be set to a non-zero
value. The IP address of the socket will then be 0.0.0.0:<IpPort>.
See [gRPC](#grpc-socket) for more information.

**(2)** To use TLS a certificate file and a corresponding private key file must
be supplied. If either is omitted, or if the device setting Use TLS is set to 'No',
TLS is not used.
See [TLS](#tls) for more information.

**(3)** When using the Machine learning API, the chip Id corresponding to the device must
be given. See [Chip id](#chip-id) for more information.

<!-- markdownlint-disable MD024 -->
#### Containerized version
<!-- markdownlint-enable MD024 -->

When starting the ACAP Runtime service from command line, as is done with the
containerized version, it accepts the following settings:

```text
-v                Verbose, enable extended logging,
-a <IP address>   IP address of gRPC server, default 0.0.0.0. See note1,
-p <IP port>      IP port of gRPC server, default 0. See note1,
-t <seconds>      Runtime in seconds (used for test),
-c <file name>    Certificate file for TLS authentication. See note2,
-k <file name>    Private key file for TLS authentication. See note2,
-j <chip id>      Chip id used by Machine learning API service. See note3,
-m <file name>    Inference model file used by Machine learning API service,
-o                Override settings from device parameters. See note4,
```

Notes.

**(1)** The gRPC server can be set up with either a unix-socket (default) or a
network socket. To set up as network socket the IP port should be set to a non-zero
value. The IP address is only used when set up as a network socket.
See [gRPC](#grpc-socket) for more information.

**(2)** To use TLS a certificate file and a corresponding private key file must
be supplied. If either is omitted TLS is not used.
See [TLS](#tls) for more information.

**(3)** When using the Machine learning API the chip Id corresponding to the device must
be given. See [Chip id](#chip-id) for more information.

**(4)** If an instance of ACAP Runtime as an ACAP application is installed on the
device, the device parameters are also available. Setting the `-o` flag will
then override the -v, -p, -j and -c and -k settings, if the the corresponding
device parameter value is valid. This setting is mainly aimed at debug/test
usage and should not be used in production.

#### Chip id

The Machine learning API uses the [Machine learning API][acap-documentation-native-ml] for image processing
and to set it up the correct chip id for the device needs to be selected.
Note that there is no direct corelation between chip id and architecture.
For convenience the pre-built images for the ACAP Runtime native application sets
the default value for ChipId to 4 for `armv7hf` and 12 for `aarch64`, since those
are currently the most common ids for the respective architectures.
See the table below for a full list of supported values.

If the value is set to 0 (LAROD_CHIP_INVALID) the Machine learning API inference service will not
be started.

| Chip id | Name                          | Description                                |
|---------|-------------------------------|--------------------------------------------|
| 0       | LAROD_CHIP_INVALID            | Invalid chip      |
| 1       | LAROD_CHIP_DEBUG              | Dummy chip for debugging |
| 4       | LAROD_CHIP_TPU                | Google TPU |
| 6       | LAROD_CHIP_CVFLOW_NN          | Ambarella CVFlow (NN) |
| 8       | LAROD_CHIP_TFLITE_GLGPU       | GPU with TensorFlow Lite. WARNING: This is an experimental chip which is subject to change. |
| 9       | LAROD_CHIP_CVFLOW_PROC        | Ambarella CVFlow (proc) |
| 10      | LAROD_CHIP_ACE                | Axis Compute Engine |
| 11      | LAROD_CHIP_LIBYUV             | CPU with libyuv. |
| 12      | LAROD_CHIP_TFLITE_ARTPEC8DLPU | ARTPEC-8 DLPU with TensorFlow Lite. WARNING: This is an experimental chip which is subject to change.|
| 13      | LAROD_CHIP_OPENCL | Image processing using OpenCL |

#### TLS

The ACAP Runtime service can be run either in TLS authenticated or unsecured mode.
TLS authenticated mode provides additional security and encryption on the gRPC
channel and is the recommended (and default) mode. The service requires a certificate
file and a key file to run in TLS authenticated mode. By default these should be
placed in the root folder of the installed application, e.g.:

```sh
/usr/local/packages/acapruntime/server.pem
/usr/local/packages/acapruntime/server.key
```

One way to generate the certificates is to use the [`openssl req`][openssl-req]
command, e.g.:

```sh
# generate the files
openssl req -x509 \
            -batch \
            -subj '/CN=localhost' \
            -days <days valid> \
            -newkey rsa:4096 \
            -nodes \
            -out server.pem \
            -keyout server.key
```

Where `<days valid>` is the number of days you want the certificate to be valid.

When using the service as an ACAP application, you generate the files on your
host computer and then transfer them to the device:

```sh
# copy the files to the device
scp server.pem server.key root@<device IP>:/usr/local/packages/acapruntime
# set correct ownership of the files on the device
ssh root@<device IP> 'chown sdk /usr/local/packages/acapruntime/server.*'
```

Where `<device IP>` is the IP address of the device.

#### gRPC socket

With the default settings the ACAP Runtime service will set at a Unix Domain Socket
(UDS) with the address:

```sh
unix:///tmp/acap-runtime.sock
```

This is suitable for projects that are contained on a device. If a network socket
is needed instead, this can be done by using the `IpPort` device parameter for the
ACAP application or the `-a` and `-p` settings for the containerized version.

### Examples

The following examples use the Parameter API with ACAP Runtime as a native
ACAP application:

- [parameter-api-cpp][paramter-api-cpp]
- [parameter-api-python][parameter-api-python]

The following examples use the ACAP Runtime containerized version to use the
Machine learning API service:

- [minimal-ml-inference][minimal-ml-inference]
- [object-detector-cpp][object-detector-cpp]

## Building ACAP Runtime

This repo provides Dockerfiles to be used to build ACAP Runtime.

<!-- markdownlint-disable MD024 -->
### Native ACAP application
<!-- markdownlint-enable MD024 -->

To build as a native ACAP application use either the Dockerfile `Dockerfile.armv7hf` or `Dockerfile.aarch64`. Select the one that matches the architecture of your device:

```sh
# Build ACAP Runtime image
docker build --file Dockerfile.<ARCH> --tag acap-runtime:<ARCH> .
```

where `<ARCH>` is either `armv7hf` or `aarch64`.

The build is based on [axisecp/acap-native-sdk][docker-hub-acap-native-sdk]. To
base it on a different version than what is on main branch you can provide the
build arguments `VERSION` and `UBUNTU_VERSION` to select a specific tag of the
`acap-native-sdk` image. E.g. to use
[axisecp/acap-native-sdk:1.4_beta1-armv7hf-ubuntu22.04][docker-hub-acap-native-sdk-1.4_beta1-armv7hf-ubuntu22.04]:

```sh
docker build --file Dockerfile.<ARCH> --tag acap-runtime:<ARCH> --build-arg VERSION=1.4beta1 --build-arg UBUNTU_VERSION=22.04 .
```

<!-- markdownlint-disable MD024 -->
### Containerized version
<!-- markdownlint-enable MD024 -->

To build the containerized version, use either `Dockerfile.armv7hf-containerized`
or `Dockerfile.aarch64-containerized`:

```sh
# Build ACAP Runtime containerized version
docker build --file Dockerfile.<ARCH>-containerized --tag acap-runtime:<ARCH>-containerized .
```

This pulls the pre-built [axisecp/acap-runtime][docker-hub-acap-runtime] image
with tag `<BUILDVERSION>-<ARCH>`. To use your own locally built ACAP Runtime image,
either re-tag it or update the Dockerfile to match. Also note that the base is an
Ubuntu image and that that version (build argument `RUNTIME_UBUNTU_VERSION`) must
match the version that the ACAP Runtime image  is based on, i.e. the ACAP native
sdk image.

## Test suite

The repo contains a test suite project to verify that ACAP Runtime works as expected
on a supported device. It builds and is executed as a standalone ACAP application
called `Acapruntimetest`.

Build and install it by running:

```sh
# Build ACAP Runtime test suite image
docker build --file Dockerfile.<ARCH> --tag acap-runtime:<ARCH>-test --build-arg TEST=yes .

docker run --rm acap-runtime:<ARCH>-test <device IP> <device password> install
```

where `<ARCH>` is either `armv7hf` or `aarch64` and `<device IP>` and `<device password>`
are the IP and root password of the device in use.

The application can be started, stopped and eventually uninstalled in the **Apps**
tab in the device GUI or by running:

```sh
docker run --rm acap-runtime:<ARCH>-test <device IP> <device password> start|stop|remove
```

To see the test run output, check the application log either by clicking on the
**App log** link in the device GUI, or directly at:

```sh
http://<device IP>/axis-cgi/admin/systemlog.cgi?appname=acapruntimetest
```

If the tests pass the log should end with \[  PASSED  ]. If any test fails, it
will be listed.

## Contributing

Take a look at the [CONTRIBUTING.md](CONTRIBUTING.md) file.

## License

[Apache 2.0](LICENSE)

<!-- Links to external references -->
<!-- markdownlint-disable MD034 -->
[acap-documentation]: https://axiscommunications.github.io/acap-documentation/docs/introduction/what-is-acap.html
[acap-documentation-native]: https://axiscommunications.github.io/acap-documentation/docs/introduction/acap-sdk-overview.html#acap-native-sdk
[acap-documentation-native-ml]: https://axiscommunications.github.io/acap-documentation/docs/api/native-sdk-api.html#machine-learning-api
[acap-documentation-cv]: https://axiscommunications.github.io/acap-documentation/docs/introduction/acap-sdk-overview.html#acap-computer-vision-sdk
[acap-documentation-acap-runtime]: https://axiscommunications.github.io/acap-documentation/docs/api/computer-vision-sdk-apis.html#beta---acap-runtime
[devices]: https://axiscommunications.github.io/acap-documentation/docs/axis-devices-and-compatibility#sdk-and-device-compatibility
[docker-acap]: https://github.com/AxisCommunications/docker-acap
[docker-hub-acap-runtime]: https://hub.docker.com/r/axisecp/acap-runtime
[docker-hub-acap-native-sdk]: https://hub.docker.com/repository/docker/axisecp/acap-native-sdk
[docker-hub-acap-native-sdk-1.4_beta1-armv7hf-ubuntu22.04]: https://hub.docker.com/layers/axisecp/acap-native-sdk/1.4_beta1-armv7hf-ubuntu22.04/images/sha256-07ed766f7a68033a2717b1334c8fdee29b1a55386b37d67924e5401c91ed9ecd?context=repo
[dockerDesktop]: https://docs.docker.com/desktop/
[dockerEngine]: https://docs.docker.com/engine/
[gRPC]: https://grpc.io/
[minimal-ml-inference]: https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/main/minimal-ml-inference
[object-detector-cpp]: https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/main/object-detector-cpp
[openssl-req]: https://www.openssl.org/docs/man3.0/man1/openssl-req.html
[parameter-api-python]: https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/main/parameter-api-python
[paramter-api-cpp]: https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/main/parameter-api-cpp
[tensorflow]: https://github.com/tensorflow/serving

<!-- markdownlint-enable MD034 -->
