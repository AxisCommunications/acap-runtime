/* Copyright 2020 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#define TESTDATA "/usr/local/packages/acapruntimetest/testdata/"

static const char* serverCertificatePath = TESTDATA "server.pem";
static const char* serverKeyPath = TESTDATA "server.key";
static const char* labelFile = TESTDATA "coco_labels.txt";
static const char* cpuModel1 = TESTDATA "ssd_mobilenet_v2_coco_quant_postprocess.tflite";
static const char* tpuModel1 = TESTDATA "ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite";
static const char* cpuModel2 = TESTDATA "mobilenet_v2_1.0_224_quant.tflite";
static const char* tpuModel2 = TESTDATA "mobilenet_v2_1.0_224_quant_edgetpu.tflite";
static const char* cpuModel3 = TESTDATA "efficientnet-edgetpu-M_quant.tflite";
static const char* tpuModel3 = TESTDATA "efficientnet-edgetpu-M_quant_edgetpu.tflite";
static const char* imageFile1 = TESTDATA "grace_hopper_300x300.bmp";
static const char* imageFile2 = TESTDATA "grace_hopper.bmp";
