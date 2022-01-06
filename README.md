# ACAP runtime
In order to make a service available to clients written in different languages or clients not even located at the same device, acap-runtime is a network protocol based service using [gRPC](https://grpc.io/) for access.

The acap-runtime service includes following services:
- Inference - An implementation of [Tensorflow Serving](https://github.com/tensorflow/serving/tree/master/tensorflow_serving/apis).
- Parameter - Axis camera parameters.

## Prerequisites
To get started following system requirements shall be met:
* Docker version 19.03.5 or higher
* Ubuntu version 20.04

## Build and run test suite for camera:
```sh
# Set your camera IP address and architecture: armv7hf or aarch64
export AXIS_TARGET_IP=<actual camera IP address>
export ARCH=armv7hf

# Build acap-runtime test suite and install and run on camera
docker build . -f Dockerfile.$ARCH --tag acap-runtime-test:$ARCH --build-arg TEST=true
docker run --rm acap-runtime-test:$ARCH $AXIS_TARGET_IP pass install
docker run --rm acap-runtime-test:$ARCH $AXIS_TARGET_IP pass start

# Find the logs in the camera web GUI and select the installed program in the Apps tab
# Terminate and unistall test suite
docker run --rm acap-runtime-test:$ARCH $AXIS_TARGET_IP pass stop
docker run --rm acap-runtime-test:$ARCH $AXIS_TARGET_IP pass remove
```

## Build and run acap runtime server for camera:
```sh
# Set your camera IP address and architecture: armv7hf or aarch64
export AXIS_TARGET_IP=<actual camera IP address>
export ARCH=armv7hf

# Build acap-runtime server and install and run on camera
docker build . -f Dockerfile.$ARCH --tag acap-runtime:$ARCH
docker run --rm acap-runtime:$ARCH $AXIS_TARGET_IP pass install
docker run --rm acap-runtime:$ARCH $AXIS_TARGET_IP pass start

# Find the logs in the camera web GUI and select the installed program in the Apps tab
# Terminate and unistall acap-runtime server
docker run --rm acap-runtime:$ARCH $AXIS_TARGET_IP pass stop
docker run --rm acap-runtime:$ARCH $AXIS_TARGET_IP pass remove
```

## Program options
Following options are available in the camera web GUI by selecting the installed program in the **Apps** tab:

```
Verbose     Enable extended logging
IpPort      IP port of server
ChipId      Chip id, see extended logging for available options
```

## License
**Apache License 2.0**