# ACAP runtime
The acap-runtime is a network protocol based service using [gRPC](https://grpc.io/) for access. This makes service available to clients written in different languages or clients not even located at the same device.

The acap-runtime service includes following services:
- Inference - An implementation of [Tensorflow Serving](https://github.com/tensorflow/serving/tree/master/tensorflow_serving/apis).
- Parameter - Axis camera parameters.

## Prerequisites
Following system requirements shall be met:
* Docker version 19.03.5 or higher
* Ubuntu version 20.04

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
# Terminate and unistall acap-runtime server
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
# Terminate and unistall test suite
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
# Terminate and unistall acap-runtime server
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
# Terminate and unistall test suite
docker run --rm acap-runtime:$ARCH-test $AXIS_TARGET_IP $PASS stop
docker run --rm acap-runtime:$ARCH-test $AXIS_TARGET_IP $PASS remove
```

## Program options

> ACAP runtime must be restarted after program options has been changed.

Following options are available in the camera web GUI after selecting the installed program in the **Apps** tab:

```
Verbose     Enable extended logging, default No
IpPort      IP port of server
ChipId      Chip id, see extended logging for available options, select 0 to disable inference server
```

## License
**Apache License 2.0**