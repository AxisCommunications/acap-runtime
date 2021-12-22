# ACAP runtime
In order to make a service available to clients written in different languages or clients not even located at the same device, acap-runtime is a network protocol based service using [gRPC](https://grpc.io/) for access.

The acap-runtime service includes following services:
- Inference - An implementation of [Tensorflow Serving](https://github.com/tensorflow/serving/tree/master/tensorflow_serving/apis).
- Parameter - Axis camera parameters.

## Prerequisites
To get started following system requirements shall be met:
* Docker version 19.03.5 or higher
* Ubuntu version 20.04
* Firmware: Axis Q1615-MkIII release 10.7
* [Docker daemon](https://github.com/AxisCommunications/docker-acap) installed on the camera with TLS enabled

## Build and run test suite
Docker build and test commands for camera (armv7hf):
```sh
# Set your camera IP address and clear docker memory
export AXIS_TARGET_IP=<actual camera IP address>
docker --tlsverify -H tcp://$AXIS_TARGET_IP:2376 system prune -af

# Set environment variables
export ARCH=armv7hf
export APP_NAME=acap-runtime

# Build acap-runtime test container image and load on camera
docker build . -t $APP_NAME:$ARCH-test -f Dockerfile.$ARCH
docker save $APP_NAME:$ARCH-test | docker --tlsverify -H tcp://$AXIS_TARGET_IP:2376 load

# Run acap-runtime test container on camera
docker --tlsverify -H tcp://$AXIS_TARGET_IP:2376 run --rm \
 --volume /usr/acap-root/lib:/host/lib \
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -it $APP_NAME:$ARCH-test acap_runtime_test

# Run test container with single test and verbose prints on camera
docker --tlsverify -H tcp://$AXIS_TARGET_IP:2376 run --rm \
 --volume /usr/acap-root/lib:/host/lib \
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -it $APP_NAME:$ARCH-test acap_runtime_test \
 --gtest_color=yes --gtest_filter=ParameterTest.GetValues
```

Docker build and test commands for desktop (amd64):
```sh
# Set environment variables
export ARCH=amd64
export APP_NAME=acap-runtime
export LAROD_VERSION=R3.0.31

# Get larod
git clone -b $LAROD_VERSION https://gittools.se.axis.com/gerrit/a/apps/larod

# Add Larod D-Bus policy
sudo cp larod/src/service/com.axis.Larod1.conf /etc/dbus-1/system.d/

# Build larod and larod-server
docker build larod -t larod:$ARCH
docker build . -f Dockerfile.larod-server-$ARCH -t larod-server:$ARCH

# Build acap-runtime test image
docker build . -f Dockerfile.$ARCH -t $APP_NAME:$ARCH-test

# Start larod server as a background process
docker run --rm --privileged\
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -itd $APP_NAME:$ARCH-test larod

# Run acap-runttime test suite
docker container run --rm --privileged\
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -it $APP_NAME:$ARCH-test acap_runtime_test
```

## Command line options
The command to run the acap-runtime server is:\
acap_runtime [-v] [-a address ] [-p port] [-j chip-id]  [-t timer] [-c certificate-file] [-k key-file] [-m model-file] ... [-m model-file]

All parameters are optional with following meaning:\
  -v    Verbose\
  -a    IP address of server\
  -p    IP port of server\
  -j    Chip id, see [larodChip](https://www.axis.com/techsup/developer_doc/acap3/3.2/api/larod/html/larod_8h.html#a5d61d65903803a3c587e5830de34df24) in larod.h, no inference if omitted\
  -t    Timer in seconds, terminates program after timer expired\
  -c    Certificate file for TLS authentication, insecure channel if omitted\
  -k    Private key file for TLS authentication, insecure channel if omitted\
  -m    Inference model file

Example of command to start the acap-runtime server on localhost port 9001:
```sh
acap_runtime -v -p 9001 -j 4 -m ssdlite-mobilenet-v2-tpu
```

## License
**Apache License 2.0**