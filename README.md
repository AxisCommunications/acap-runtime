ACAP runtime
======================
In order to make larod also available to clients that do not reside on the very
same device, which is the case in many real machine learning applications,
we need to supply a network protocol based interface. The standard way to do
this is to provide an inference server.

## API
The larod inference server is an implementation of the API in [Tensorflow Serving](https://github.com/tensorflow/serving/tree/master/tensorflow_serving/apis) and uses [gRPC](https://grpc.io/) for access.

## Architecture
The main file [acap-runtime.cpp](src/acap-runtime.cpp)
starts the service using the front-end files [inference.h](src/inference.h) and
 [inference.cpp](src/inference.cpp).
Incoming requests are translated to larod requests in order to get the inference results.
They that are then relayed back to the requester.

## Prerequisites
To get started following system requirements shall be met:
* Docker version 19.03.5 or higher
* Ubuntu version 20.04
* Firmware: Axis Q1615-MkIII release 10.7
* Docker Daemon installed on the camera

## Build
Example docker build command for camera:
```sh
# Build docker runtime container image for camera
docker build . -t axisecp/acap-runtime:latest -f Dockerfile.armv7hf --target runtime
docker push axisecp/acap-runtime:latest

# Build docker test container image for camera
docker build . -t axisecp/acap-runtime:latest-test -f Dockerfile.armv7hf
docker push axisecp/acap-runtime:latest-test

# Run test container on camera, example:
export AXIS_TARGET_IP=<IP Address>
docker -H tcp://$AXIS_TARGET_IP system prune -af
docker -H tcp://$AXIS_TARGET_IP pull axisecp/acap-runtime:latest-test
docker -H tcp://$AXIS_TARGET_IP run --rm --volume /usr/acap-root/lib:/host/lib \
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket -it axisecp/acap-runtime:latest-test acap-runtime.test

# Run test container with verbose prints on camera
docker -H tcp://$AXIS_TARGET_IP run --rm --volume /usr/acap-root/lib:/host/lib \
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -it axisecp/acap-runtime:latest-test acap-runtime.test --gtest_color=yes --gtest_filter=ParameterTest.GetValues
```

Example docker build command for desktop:
```sh
# Get acap-runtime and larod
export VERSION=0.1
export LAROD_VERSION=R2.2.39
git clone -b $VERSION https://gittools.se.axis.com/gerrit/a/apps/acap-runtime.git
cd acap-runtime
git clone -b $LAROD_VERSION https://gittools.se.axis.com/gerrit/a/apps/larod

# Add Larod D-Bus policy
sudo cp src/service/com.axis.Larod1.conf /etc/dbus-1/system.d/

# Build larod and larod-server
docker build ./larod -t larod
docker build . -f Dockerfile.larod-server -t larod-server

# Build acap-runtime
docker image build . -f Dockerfile.desktop -t acap-runtime:$VERSION.test-amd64
```

Run inference server integration and unit tests.
Start larod server on desktop in a separate window:
```sh
export VERSION=0.1
docker run --rm --privileged\
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -it acap-runtime:$VERSION.test-amd64 larod
```

Test acap-runtime by running the test suite:
```sh
docker container run --rm --privileged\
 --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket \
 -it acap-runtime:$VERSION.test-amd64 acap-runtime.test --gtest_color=yes --gtest_filter=Inference.PredictCpuModel1
```

Example build command for desktop debug:
```sh
# Start larod docker server in a separate window
docker run --privileged --volume /run/dbus/system_bus_socket:/run/dbus/system_bus_socket -it larod-desktop

# Build and install release and test on desktop
sudo make clean
sudo make install
sudo make test

# Run test on desktop
sudo ./build_x86_64-linux-gnu/acap-runtime.test

# Run test with verbose prints
sudo ./build_x86_64-linux-gnu/acap-runtime.test --gtest_color=yes

# Run specific test 1000 times
sudo ./build_x86_64-linux-gnu/acap-runtime.test --gtest_repeat=1000 --gtest_filter=Inference.PredictTpu

# Run test with valgrind memory leak tool
sudo valgrind --leak-check=full --show-leak-kinds=all --log-file=leak.txt ./build_x86_64-linux-gnu/acap-runtime.test
```

## Run
The command to run the inference server is:\
acap-runtime [-v] [-a address ] [-p port] [-j chip-id]  [-t runtime] [-c certificate-file] [-k key-file] [-m model-file] ... [-m model-file]

All parameters are optional with following meaning:\
  -v    Verbose\
  -a    IP address of server\
  -p    IP port of server\
  -j    Chip id see [larodChip](https://www.axis.com/techsup/developer_doc/acap3/3.2/api/larod/html/larod_8h.html#a5d61d65903803a3c587e5830de34df24) in larod.h\
  -t    Runtime in seconds (used for test)\
  -c    Certificate file for TLS authentication, insecure channel if omitted\
  -k    Private key file for TLS authentication, insecure channel if omitted\
  -m    Larod model file\

Example of command to start the inference server on localhost port 9001:
```sh
acap-runtime -v -p 9001 -j 4 -m ssdlite-mobilenet-v2-tpu
```

## Server Authentication
The API uses an insecure gRPC communication channel, but it is possible to activate SSL/TLS server authentication and encryption. When SSL/TLS is activated, a certificate and private key for your organization must be provided to the inference server. Here is an example how to generate a temporary test certificate:
```sh
# Generate TSL/SSL test certificate
# Press default for all input except: Common Name (e.g. server FQDN or YOUR name) []:localhost
openssl req -x509 -days 10000 -newkey rsa:4096 -nodes -writerand ~/.rnd -out server.pem -keyout server.key
```

Update file test/test_certificate.h with content from file testdata/server.pem.

The inference server must be started by specifying the certificate and the private key:
```sh
acap-runtime -v -p 9001 -j 4 -c server.pem -k server.key -m ssdlite-mobilenet-v2-tpu
```

## License
**Apache License 2.0**