*Copyright (C) 2022, Axis Communications AB, Lund, Sweden. All Rights Reserved.*

# BETA - ACAP runtime

The acap-runtime is a network protocol based service using [gRPC](https://grpc.io/) and Unix Domain Socket (UDS) for access. This makes the service available to clients written in different languages on the same device.

The acap-runtime service includes the following services:

- Inference - An implementation of [Tensorflow Serving](https://github.com/tensorflow/serving/tree/master/tensorflow_serving/apis).
- Parameter - Axis camera parameters.

## Prerequisites

Following system requirements shall be met:

- Docker version 19.03.5 or higher
- Ubuntu version 20.04
- Certificate files if TLS is used

## Run latest prebuilt acap runtime server on camera

```sh
# Set your camera architecture and IP address
export ARCH=<armv7hf or aarch64>
export AXIS_TARGET_IP=<camera IP address>
export PASS=<camera password> # Enclose with ' if password contains special characters

# Install and run acap-runtime server on camera
docker run --rm axisecp/acap-runtime:$ARCH $AXIS_TARGET_IP $PASS install
docker run --rm axisecp/acap-runtime:$ARCH $AXIS_TARGET_IP $PASS start

# Find the logs in the camera web GUI and select the installed program in the Apps tab
# Terminate and uninstall acap-runtime server
docker run --rm axisecp/acap-runtime:$ARCH $AXIS_TARGET_IP $PASS stop
docker run --rm axisecp/acap-runtime:$ARCH $AXIS_TARGET_IP $PASS remove
```

## Run latest prebuilt test suite on camera

```sh
# Set your camera architecture and IP address
export ARCH=<armv7hf or aarch64>
export AXIS_TARGET_IP=<camera IP address>
export PASS=<camera password> # Enclose with ' if password contains special characters

# Install and run test suite on camera
docker run --rm axisecp/acap-runtime:$ARCH-test $AXIS_TARGET_IP $PASS install
docker run --rm axisecp/acap-runtime:$ARCH-test $AXIS_TARGET_IP $PASS start

# Find the logs in the camera web GUI and select the installed program in the Apps tab
# Terminate and uninstall test suite
docker run --rm axisecp/acap-runtime:$ARCH-test $AXIS_TARGET_IP $PASS stop
docker run --rm axisecp/acap-runtime:$ARCH-test $AXIS_TARGET_IP $PASS remove
```

## Build and run acap runtime server on camera

This section requires access to acap-runtime GitHub repo.

```sh
# Set your camera architecture and IP address
export ARCH=<armv7hf or aarch64>
export AXIS_TARGET_IP=<camera IP address>
export PASS=<camera password> # Enclose with ' if password contains special characters

# Build acap-runtime server and install and run on camera
docker build . -f Dockerfile.$ARCH --tag acap-runtime:$ARCH
docker run --rm acap-runtime:$ARCH $AXIS_TARGET_IP $PASS install
docker run --rm acap-runtime:$ARCH $AXIS_TARGET_IP $PASS start

# Find the logs in the camera web GUI and select the installed program in the Apps tab
# Terminate and uninstall acap-runtime server
docker run --rm acap-runtime:$ARCH $AXIS_TARGET_IP $PASS stop
docker run --rm acap-runtime:$ARCH $AXIS_TARGET_IP $PASS remove
```

## Build and run test suite on camera

This section requires access to acap-runtime GitHub repo.

```sh
# Set your camera architecture and IP address
export ARCH=<armv7hf or aarch64>
export AXIS_TARGET_IP=<camera IP address>
export PASS=<camera password> # Enclose with ' if password contains special characters

# Build acap-runtime test suite and install and run on camera
docker build . -f Dockerfile.$ARCH --tag acap-runtime:$ARCH-test --build-arg TEST=true
docker run --rm acap-runtime:$ARCH-test $AXIS_TARGET_IP $PASS install
docker run --rm acap-runtime:$ARCH-test $AXIS_TARGET_IP $PASS start

# Find the logs in the camera web GUI and select the installed program in the Apps tab
# Terminate and uninstall test suite
docker run --rm acap-runtime:$ARCH-test $AXIS_TARGET_IP $PASS stop
docker run --rm acap-runtime:$ARCH-test $AXIS_TARGET_IP $PASS remove
```

## Build containerized version
This section requires access to acap-runtime GitHub repo.

```sh
# Set your camera architecture
export ARCH=<armv7hf or aarch64>

# Build acap-runtime containerized version
docker build . -f Dockerfile.containerized --tag acap-runtime:$ARCH-containerized --build-arg ARCH=$ARCH

```

## Program options

> ACAP runtime must be restarted after program options has been changed.

Following options are available in the camera web GUI after selecting the installed program in the **Apps** tab:

```
ChipId      Chip id, see extended logging for available options, select 0 to disable inference server
Use TLS     Enable SSL/TLS, default No
Verbose     Enable extended logging, default No
```

## Use TLS

This service can be run either unsecured or in TLS mode. TLS mode provides additional security and encryption on the gRPC channel. There is a "Use TLS" dropdown in the web interface to switch between the two different modes. Note that the service has to be restarted every time TLS is activated or deactivated. TLS requires certificate and key file to work, which are listed below. For more information on how to generate these files, please see the [Parameter API](https://github.com/AxisCommunications/acap-computer-vision-sdk-examples/tree/master/parameter-api) example.

```
/usr/local/packages/acapruntime/server.pem
/usr/local/packages/acapruntime/server.key
```

## Access point

The acap-runtime is a network protocol based service using [gRPC](https://grpc.io/) and the access point is an Unix Domain Socket (UDS).

```
acap-runtime.sock
```

## License

[Apache 2.0](LICENSE)
