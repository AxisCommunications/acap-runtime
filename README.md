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
* Docker Daemon installed on the camera

## Build and run test suite
Docker build commands for camera (armv7hf):
```sh
# Build acap-runtime container image for camera
docker build . -t axisecp/acap-runtime:latest-armv7hf -f Dockerfile.armv7hf --target runtime
docker push axisecp/acap-runtime:latest-armv7hf

# Build acap-runtime test container image for camera
docker build . -t axisecp/acap-runtime:latest-armv7hf-test -f Dockerfile.armv7hf
docker push axisecp/acap-runtime:latest-armv7hf-test

# Run acap-runtime test container on camera
export AXIS_TARGET_IP=<IP Address>
docker -H tcp://$AXIS_TARGET_IP system prune -af
docker -H tcp://$AXIS_TARGET_IP pull axisecp/acap-runtime:latest-armv7hf-test
docker -H tcp://$AXIS_TARGET_IP run --rm --volume /usr/acap-root/lib:/host/lib \
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -it axisecp/acap-runtime:latest-armv7hf-test acap-runtime.test

# Run test container with verbose prints on camera
docker -H tcp://$AXIS_TARGET_IP run --rm --volume /usr/acap-root/lib:/host/lib \
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -it axisecp/acap-runtime:latest-armv7hf-test acap-runtime.test \
 --gtest_color=yes --gtest_filter=ParameterTest.GetValues
```

Docker build command for desktop (amd64):
```sh
# Add Larod D-Bus policy
sudo cp src/service/com.axis.Larod1.conf /etc/dbus-1/system.d/

# Get larod
export LAROD_VERSION=R3.0.31
git clone -b $LAROD_VERSION https://gittools.se.axis.com/gerrit/a/apps/larod

# Build larod and larod-server
docker build ./larod -t larod
docker build . -f Dockerfile.larod-server -t larod-server

# Build acap-runtime
docker build . -f Dockerfile.amd64 -t acap-runtime:latest-amd64-test
```

Run acap-runtime server integration and unit tests on desktop computer.
```sh
# Start larod server in a separate window
docker run --rm --privileged\
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -it acap-runtime:latest-amd64-test larod
```

Test acap-runtime by running the test suite:
```sh
# Run test suite in another window
docker container run --rm --privileged\
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -it acap-runtime:latest-amd64-test acap-runtime.test
```

## Command line options
The command to run the acap-runtime server is:\
acap-runtime [-v] [-a address ] [-p port] [-j chip-id]  [-t timer] [-c certificate-file] [-k key-file] [-m model-file] ... [-m model-file]

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
acap-runtime -v -p 9001 -j 4 -m ssdlite-mobilenet-v2-tpu
```

## License
**Apache License 2.0**