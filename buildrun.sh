#!/bin/bash

cam=192.168.0.13
port=1234

# model=ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite
model=mobilenet_v2_1.0_224_quant_edgetpu.tflite

width=224
height=224


# Build and install ACAPs

. /opt/axis/acapsdk/environment-setup-cortexa9hf-neon-poky-linux-gnueabi
set -x

acap-build . -m manifest-armv7hf.json

# eap-install.sh $cam pass install

# CXXFLAGS="$CXXFLAGS -g0 -DTEST" acap-build . -m manifest-test.json -a 'testdata/*'
# rm ACAP_Runtime_1_1_0_armv7hf.eap
# eap-install.sh $cam pass install

# Copy binaries to device

sshpass -p pass ssh root@$cam 'killall acapruntime'
sshpass -p pass ssh root@$cam 'killall acapruntimetest'

sshpass -p pass scp ./acapruntime root@$cam:/usr/local/packages/acapruntime/acapruntime
# sshpass -p pass scp ./acapruntimetest root@$cam:/usr/local/packages/acapruntimetest/acapruntimetest 

# Perform grpcurl tests

sshpass -p pass ssh root@$cam "/usr/local/packages/acapruntime/acapruntime -v -p $port" &
sleep 2

cp apis/tensorflow_serving/apis/prediction_service.proto ./apis

# # Format 2 is jpg, format 3 is yuv
apis/grpcurl --import-path /opt/app_host/apis --proto videocapture.proto --plaintext -d '{"settings": { "format": "3", "width": '$width', "height": '$height', "framerate": 10, "timestamp_type": "1" }}' \
 $cam:$port videocapture.VideoCapture/NewStream | jq --raw-output .streamId > temp

stream=$(cat temp)
rm temp

# read -n 1 -p "Press key"

# apis/grpcurl --import-path /opt/app_host/apis --proto videocapture.proto --plaintext -d '{ "stream_id": '$stream'}' $cam:$port videocapture.VideoCapture/GetFrame \
#  | jq --raw-output .data | base64 --decode > img.yuv

apis/grpcurl --import-path /opt/app_host/apis --proto prediction_service.proto --plaintext -d \
 '{ "inputs": { "data": { "dtype": 22, "uint32_val": '$stream', "tensor_shape": { "dim": [{"size": 1}, {"size": '$width'}, {"size": '$height'}, {"size": 2}] } }  }, "model_spec": { "name": "/var/spool/storage/SD_DISK/models/'$model'"  }  }' \
 $cam:$port tensorflow.serving.PredictionService/Predict \
 | tee /dev/stderr \
 | jq --raw-output '.outputs."MobilenetV2/Predictions/Softmax".tensorContent' \
 | base64 --decode \
 | od --format u1 -A d 

# | tail -c 100

rm ./apis/prediction_service.proto

sshpass -p pass ssh root@$cam "killall acapruntime"

# Run test binary

# sshpass -p pass ssh root@$cam "/usr/local/packages/acapruntimetest/acapruntimetest --gtest_filter='-Inference*'" & #--gtest_color=yes

# sshpass -p pass ssh root@$192.168.0.13 "killall acapruntimetest"
