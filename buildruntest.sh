#!/bin/bash

cam=192.168.0.13
port=1234

# model=ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite
model=mobilenet_v2_1.0_224_quant_edgetpu.tflite

width=224
height=224


# Build and install ACAPs

. /opt/axis/acapsdk/environment-setup-cortexa9hf-neon-poky-linux-gnueabi
set 

# acap-build . -m manifest-armv7hf.json

# eap-install.sh $cam pass install

CXXFLAGS="$CXXFLAGS -g0 -DTEST" acap-build . -m manifest-test.json -a 'testdata/*'
# rm ACAP_Runtime_1_1_0_armv7hf.eap
# eap-install.sh $cam pass install

# Copy binaries to device

# sshpass -p pass ssh root@$cam 'killall acapruntime'
sshpass -p pass ssh root@$cam 'killall acapruntimetest'

sshpass -p pass scp ./acapruntimetest root@$cam:/usr/local/packages/acapruntimetest/acapruntimetest 

# Run test binary

sshpass -p pass ssh root@$cam "/usr/local/packages/acapruntimetest/acapruntimetest --gtest_filter='-InferenceUnit*' --gtest_color=yes" # --gtest_filter='-Inference*' --gtest_color=yes

sshpass -p pass ssh root@$cam "killall acapruntimetest"
