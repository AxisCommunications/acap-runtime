# syntax=docker/dockerfile:1

ARG ARCH=armv7hf
ARG REPO=axisecp
ARG VERSION=1.14
ARG UBUNTU_VERSION=22.04
ARG GRPC_VERSION=v1.65.5

# hadolint ignore=DL3029
FROM --platform=linux/arm64/v8 ubuntu:${UBUNTU_VERSION} AS containerized_aarch64
# hadolint ignore=DL3029
FROM --platform=linux/arm/v7 ubuntu:${UBUNTU_VERSION} AS containerized_armv7hf

FROM ${REPO}/acap-native-sdk:${VERSION}-${ARCH}-ubuntu${UBUNTU_VERSION} AS acap-native-sdk

FROM acap-native-sdk AS build_base

ARG ARCH
ARG TARGETSYSROOT=/opt/axis/acapsdk/sysroots/${ARCH}

# Remove Abseil dynamic libs from SDK since dependencies should link to static libs
WORKDIR ${TARGETSYSROOT}/usr/lib
RUN [ -z "$(ls libabsl*.so*)" ] || rm -f libabsl*.so*

# Install openssl (to use instead of boringssl)
# hadolint ignore=DL3009
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

FROM build_base AS tensorflow

WORKDIR /opt

# Get TensorFlow and TensorFlow Serving - doing this in parallel saves a little bit of time
RUN <<EOF
    git clone -b r2.9 https://github.com/tensorflow/tensorflow.git /opt/tensorflow/tensorflow
    git clone -b r2.9 https://github.com/tensorflow/serving.git /opt/tensorflow/serving
EOF

FROM build_base AS testdata

# Install Edge TPU compiler
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
# hadolint ignore=DL3009
RUN <<EOF
echo "deb https://packages.cloud.google.com/apt coral-edgetpu-stable main" \
    | tee /etc/apt/sources.list.d/coral-edgetpu.list
curl -k https://packages.cloud.google.com/apt/doc/apt-key.gpg | apt-key add -
apt-get update
apt-get install -y --no-install-recommends \
    edgetpu-compiler
EOF

# Get testdata models
WORKDIR /opt/app/testdata

# Generate TSL/SSL test certificate
RUN openssl req -x509 -batch -subj '/CN=localhost' -days 10000 -newkey rsa:4096 -nodes -out server.pem -keyout server.key

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

FROM build_base AS build_grpc

ARG ARCH
ARG TARGETSYSROOT=/opt/axis/acapsdk/sysroots/${ARCH}
ARG GRPC_VERSION

# Switch to build directory
WORKDIR /opt

# Build and install gRPC for the host architecture.
# We do this because we need to be able to run protoc and grpc_cpp_plugin
# while cross-compiling.
RUN <<EOF
    git clone -b ${GRPC_VERSION} https://github.com/grpc/grpc
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
    make "-j$(nproc)" install
EOF

FROM build_grpc AS build_grpc_arm

ARG ARCH
ARG TARGETSYSROOT=/opt/axis/acapsdk/sysroots/${ARCH}

# return to build dir
WORKDIR /opt

# Build and install gRPC for ARM.
# This build will use the host architecture copies of protoc and
# grpc_cpp_plugin that we built earlier because we installed them
# to a location in our PATH (/usr/local/bin).
WORKDIR /opt/grpc/cmake/build_arm
RUN <<EOF
    export SYSTEM_PROCESSOR_ARCH="$ARCH";
    . /opt/axis/acapsdk/environment-setup*
    CXXFLAGS="$CXXFLAGS -g0" cmake \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR="$SYSTEM_PROCESSOR_ARCH" \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
        -DCMAKE_INSTALL_PREFIX="$SDKTARGETSYSROOT"/usr \
        -DCMAKE_FIND_ROOT_PATH="$SDKTARGETSYSROOT"/usr \
        -DCMAKE_BUILD_TYPE=Release \
        ../..
    make "-j$(nproc)" install/strip
    cp -r /opt/grpc/third_party/googletest/googletest/include/gtest \
        "$SDKTARGETSYSROOT"/usr/include
EOF

FROM build_grpc_arm AS build

ARG ARCH
ARG TARGETSYSROOT=/opt/axis/acapsdk/sysroots/${ARCH}
ARG TEST
ARG DEBUG

# Get TensorFlow, TensorFlow Serving and testdata that we pulled in paralell
COPY --from=tensorflow /opt/tensorflow /opt/tensorflow
COPY --from=testdata /opt/app/testdata /opt/app/testdata

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

# Building the ACAP application
# hadolint ignore=SC2046,SC2155
RUN <<EOF
    export MANIFEST="manifest-$ARCH.json";
    export EXTRA_FLAGS=$([ "$ARCH" = "aarch64" ] && echo "-D__arm64__" || echo "");
    . /opt/axis/acapsdk/environment-setup*
    if [ -n "$DEBUG" ]; then
        printf "Building debug\n"
        CXXFLAGS="$CXXFLAGS -O0 -ggdb $EXTRA_FLAGS" \
        acap-build . -m "$MANIFEST"
    elif [ -n "$TEST" ]; then
        printf "Building test\n"
        CXXFLAGS="$CXXFLAGS -g0 $EXTRA_FLAGS -DTEST" \
        acap-build . -m manifest-test.json -a 'testdata/*'
    else
        printf "Building app\n"
        CXXFLAGS="$CXXFLAGS -g0 $EXTRA_FLAGS" \
        acap-build . -m "$MANIFEST"
    fi
EOF

# Copy out eap to an installation image
# Use this to install ACAP Runtime as an ACAP on a device
FROM acap-native-sdk AS runtime-base

WORKDIR /opt/app
COPY --from=build /opt/app/*.eap ./
COPY --from=build /opt/app/*.conf ./

ENTRYPOINT [ "/opt/axis/acapsdk/sysroots/x86_64-pokysdk-linux/usr/bin/eap-install.sh" ]

# Copy out eap to a containerized image
# Use this to run ACAP Runtime in a container on a device
# hadolint ignore=DL3006
FROM containerized_${ARCH} AS containerized

WORKDIR /opt/app/
COPY --from=runtime-base /opt/app/*.eap ./
RUN <<EOF
    mkdir -p acap_runtime
    for f in *.eap; do
        tar -xzf "$f" -C acap_runtime
    done
EOF

ENTRYPOINT [ "/opt/app/acap_runtime/acapruntime" ]

FROM scratch AS binaries

COPY --from=runtime-base /opt/app/*.eap /
