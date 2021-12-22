/* Copyright 2020 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#ifndef EXEROOT
#define EXEROOT
#endif

// Add quotes to string
#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

static const char* serverCertificatePath = STRINGIZE_VALUE_OF(EXEROOT) "testdata/server.pem";
static const char* serverKeyPath = STRINGIZE_VALUE_OF(EXEROOT) "testdata/server.key";
static const char* labelFile = STRINGIZE_VALUE_OF(EXEROOT) "testdata/coco_labels.txt";
static const char* cpuModel1 = STRINGIZE_VALUE_OF(EXEROOT) "testdata/ssd_mobilenet_v2_coco_quant_postprocess.tflite";
static const char* tpuModel1 = STRINGIZE_VALUE_OF(EXEROOT) "testdata/ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite";
static const char* cpuModel2 = STRINGIZE_VALUE_OF(EXEROOT) "testdata/mobilenet_v2_1.0_224_quant.tflite";
static const char* tpuModel2 = STRINGIZE_VALUE_OF(EXEROOT) "testdata/mobilenet_v2_1.0_224_quant_edgetpu.tflite";
static const char* cpuModel3 = STRINGIZE_VALUE_OF(EXEROOT) "testdata/efficientnet-edgetpu-M_quant.tflite";
static const char* tpuModel3 = STRINGIZE_VALUE_OF(EXEROOT) "testdata/efficientnet-edgetpu-M_quant_edgetpu.tflite";
static const char* imageFile1 = STRINGIZE_VALUE_OF(EXEROOT) "testdata/grace_hopper_300x300.bmp";
static const char* imageFile2 = STRINGIZE_VALUE_OF(EXEROOT) "testdata/grace_hopper.bmp";
