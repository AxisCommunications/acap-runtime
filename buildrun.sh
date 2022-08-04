#!/bin/bash

set -x

cam=192.168.0.13
port=1234

. /opt/axis/acapsdk/environment-setup-cortexa9hf-neon-poky-linux-gnueabi && acap-build . -m manifest-armv7hf.json

sshpass -p pass ssh root@$cam 'killall acapruntime'

sshpass -p pass scp ./acapruntime root@$cam:/usr/local/packages/acapruntime/acapruntime

sshpass -p pass ssh root@$cam "/usr/local/packages/acapruntime/acapruntime -v -p $port" &
sleep 2

cp apis/tensorflow_serving/apis/prediction_service.proto ./apis

apis/grpcurl --import-path /opt/app_host/apis --proto videocapture.proto --plaintext -d '{"settings": { "format": "3", "width": 300, "height": 300, "framerate": 10, "timestamp_type": "1" }}' \
 $cam:$port videocapture.VideoCapture/NewStream | jq --raw-output .streamId > temp

stream=$(cat temp)
rm temp

# read -n 1 -p "Press key"

apis/grpcurl --import-path /opt/app_host/apis --proto prediction_service.proto --plaintext -d '{ "inputs": { "data": { "dtype": 22, "uint32_val": '$stream', "tensor_shape": { "dim": [{"size": 1}, {"size": 300}, {"size": 300}, {"size": 2}] } }  }, "model_spec": { "name": "/var/spool/storage/SD_DISK/models/ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite"  }  }' $cam:$port tensorflow.serving.PredictionService/Predict

apis/grpcurl --import-path /opt/app_host/apis --proto videocapture.proto --plaintext -d '{ "stream_id": '$stream'}' $cam:$port videocapture.VideoCapture/GetFrame | tail -c 100

rm ./apis/prediction_service.proto