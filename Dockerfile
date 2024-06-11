# syntax=docker/dockerfile:1

ARG ARCH=aarch64
ARG REPO=axisecp
ARG VERSION=1.7
ARG UBUNTU_VERSION=22.04

FROM arm64v8/ubuntu:${UBUNTU_VERSION} as containerized_aarch64
FROM arm32v7/ubuntu:${UBUNTU_VERSION} as containerized_armv7hf

FROM ${REPO}/acap-native-sdk:${VERSION}-${ARCH}-ubuntu${UBUNTU_VERSION} AS acap-native-sdk
FROM acap-native-sdk AS build-image

ARG ARCH
ARG TARGETSYSROOT=/opt/axis/acapsdk/sysroots/${ARCH}

# Remove Abseil dynamic libs from SDK since dependencies should link to static libs
WORKDIR ${TARGETSYSROOT}/usr/lib
RUN [ -z "$(ls libabsl*.so*)" ] || rm -f libabsl*.so*

# Install openssl (to use instead of boringssl)
RUN <<EOF
apt-get update
apt-get install -y --no-install-recommends \
    pkg-config \
    g++ \
    cmake \
    libssl-dev \
    gnupg \
    openssl
EOF

# Install Edge TPU compiler
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
RUN <<EOF
echo "deb https://packages.cloud.google.com/apt coral-edgetpu-stable main" \
    | tee /etc/apt/sources.list.d/coral-edgetpu.list
curl -k https://packages.cloud.google.com/apt/doc/apt-key.gpg | apt-key add -
apt-get update
apt-get install -y --no-install-recommends \
    edgetpu-compiler
EOF

FROM build-image AS test-image

# Get testdata models
WORKDIR /opt/app/testdata

# Generate TSL/SSL test certificate
#RUN openssl req -x509 -batch -subj '/CN=localhost' -days 10000 -newkey rsa:4096 -nodes -out server.pem -keyout server.key

# Get SSD Mobilenet V2
ADD https://github.com/google-coral/edgetpu/raw/master/test_data/ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite .
ADD https://github.com/google-coral/edgetpu/raw/master/test_data/ssd_mobilenet_v2_coco_quant_postprocess.tflite .
ADD https://github.com/google-coral/edgetpu/raw/master/test_data/coco_labels.txt .
ADD https://github.com/google-coral/edgetpu/raw/master/test_data/grace_hopper.bmp .

# Get Mobilenet V2
ADD http://download.tensorflow.org/models/tflite_11_05_08/mobilenet_v2_1.0_224_quant.tgz tmp/
ADD https://github.com/google-coral/edgetpu/raw/master/test_data/mobilenet_v2_1.0_224_quant_edgetpu.tflite .
ADD https://github.com/google-coral/edgetpu/raw/master/test_data/imagenet_labels.txt .
RUN <<EOF
cd tmp
tar -xvf mobilenet_v2_1.0_224_quant.tgz
mv ./*.tflite ..
cd ..
rm -rf tmp
EOF

# Get EfficientNet-EdgeTpu (M)
ADD https://storage.googleapis.com/cloud-tpu-checkpoints/efficientnet/efficientnet-edgetpu-M.tar.gz tmp/
RUN <<EOF
cd tmp
tar -xvf efficientnet-edgetpu-M.tar.gz
cd efficientnet-edgetpu-M
edgetpu_compiler --min_runtime_version 13 efficientnet-edgetpu-M_quant.tflite
mv efficientnet-edgetpu-M_quant*.tflite ../..
cd ../..
rm -rf tmp
EOF

FROM build-image AS grpc-image

# Switch to build directory
WORKDIR /opt

# Build and install gRPC for the host architecture.
# We do this because we need to be able to run protoc and grpc_cpp_plugin
# while cross-compiling.
RUN <<EOF
git clone -b v1.46.3 https://github.com/grpc/grpc
cd grpc
git submodule update --init
EOF

#Quick patch security warning
RUN sed -i 's/gpr_log(GPR_INFO, __func__);/gpr_log(GPR_INFO, "%s", __func__);/g' \
    /opt/grpc/src/core/ext/transport/binder/transport/binder_transport.cc \
    /opt/grpc/src/core/ext/transport/binder/wire_format/wire_reader_impl.cc

# Build and install gRPC for the host architecture.
# We do this because we need to be able to run protoc and grpc_cpp_plugin
# while cross-compiling.
WORKDIR /opt/grpc/cmake/build
RUN <<EOF
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DgRPC_INSTALL=ON \
    -DgRPC_BUILD_TESTS=OFF \
    -DgRPC_SSL_PROVIDER=package \
    ../..
make -j4 install
EOF

FROM grpc-image AS dependency-image

ARG ARCH

# return to build dir
WORKDIR /opt
# Build for ARM

# Clone openssl and extract source code
RUN <<EOF
curl -O https://www.openssl.org/source/openssl-1.1.1l.tar.gz
tar xzvf openssl-1.1.1l.tar.gz
mkdir -p openssl-1.1.1l/build
cd openssl-1.1.1l/build
rm -rf ../doc
../Configure linux-armv4 no-asm --prefix=$TARGETSYSROOT/usr
if [ "$ARCH" = "armv7hf" ]; then
    make CC=arm-linux-gnueabihf-gcc
elif [ "$ARCH" = "aarch64" ]; then
    make CC=aarch64-linux-gnu-gcc;
fi;
make install
EOF

# Build and install gRPC for ARM.
# This build will use the host architecture copies of protoc and
# grpc_cpp_plugin that we built earlier because we installed them
# to a location in our PATH (/usr/local/bin).
WORKDIR /opt/grpc/cmake/build_arm
RUN <<EOF
. /opt/axis/acapsdk/environment-setup*
CXXFLAGS="$CXXFLAGS -g0" cmake \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR="$ARCH" \
    -DCMAKE_INSTALL_PREFIX="$SDKTARGETSYSROOT"/usr \
    -DCMAKE_FIND_ROOT_PATH="$SDKTARGETSYSROOT"/usr \
    -DgRPC_INSTALL=ON \
    -DgRPC_SSL_PROVIDER=package \
    -DCMAKE_BUILD_TYPE=Release \
    ../..
make -j4 install/strip
cp -r /opt/grpc/third_party/googletest/googletest/include/gtest \
    "$SDKTARGETSYSROOT"/usr/include
EOF

# Get TensorFlow and TensorFlow Serving
RUN <<EOF
git clone -b r2.9 https://github.com/tensorflow/tensorflow.git /opt/tensorflow/tensorflow
git clone -b r2.9 https://github.com/tensorflow/serving.git /opt/tensorflow/serving
EOF

FROM dependency-image AS build

ARG ARCH
ENV ACAPARCH=${ARCH}

## Setup build structure
WORKDIR /opt/app

COPY . .
RUN <<EOF
cd apis
ln -fs /opt/tensorflow/tensorflow/tensorflow .
ln -fs /opt/tensorflow/serving/tensorflow_serving .
EOF

# Patch the Predict call of TensorFlow Serving
RUN patch /opt/app/apis/tensorflow_serving/apis/predict.proto /opt/app/apis/predict_additions.patch

COPY --from=test-image /opt/app/testdata/* /opt/app/testdata/.

# Building the ACAP application
ARG TEST
ARG DEBUG
RUN <<EOF
if [ "$ARCH" = "armv7hf" ]; then
    export CXXFLAGS_DEBUG="$CXXFLAGS -O0 -ggdb"
    export CXXFLAGS_TEST="$CXXFLAGS -g0 -DTEST"
    export CXXFLAGS_BUILD="$CXXFLAGS -g0"
elif [ "$ARCH" = "aarch64" ]; then
    export CXXFLAGS_DEBUG="$CXXFLAGS -O0 -ggdb -D__arm64__"
    export CXXFLAGS_TEST="$CXXFLAGS -g0 -D__arm64__ -DTEST"
    export CXXFLAGS_BUILD="$CXXFLAGS -g0 -D__arm64__"
fi;
. /opt/axis/acapsdk/environment-setup*
if [ -n "$DEBUG" ]; then
    printf "Building debug\n"
    CXXFLAGS="$CXXFLAGS_DEBUG" \
    acap-build . -m manifest-"$ACAPARCH".json
elif [ -n "$TEST" ]; then
    printf "Building test\n"
    CXXFLAGS="$CXXFLAGS_TEST" \
    acap-build . -m manifest-test.json -a 'testdata/*'
else
    printf "Building app\n"
    CXXFLAGS="$CXXFLAGS_BUILD" \
    acap-build . -m manifest-"$ACAPARCH".json
fi
EOF

# Copy out eap to an installation image
# Use this to install ACAP Runtime as an ACAP on a device
FROM acap-native-sdk as runtime-base

WORKDIR /opt/app
COPY --from=build /opt/app/*.eap ./
COPY --from=build /opt/app/*.conf ./

ENTRYPOINT [ "/opt/axis/acapsdk/sysroots/x86_64-pokysdk-linux/usr/bin/eap-install.sh" ]

# Copy out eap to a containerized image
# Use this to run ACAP Runtime in a container on a device
FROM containerized_${ARCH}
WORKDIR /opt/app/
COPY --from=runtime-base /opt/app/*.eap ./
RUN <<EOF
mkdir -p acap_runtime
for f in *.eap; do
    tar -xzf "$f" -C acap_runtime
done
EOF

ENTRYPOINT [ "/opt/app/acap_runtime/acapruntime" ]
