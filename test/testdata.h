/**
 * Copyright (C) 2022 Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define TESTDATA "/usr/local/packages/acapruntimetest/testdata/"

static const char* target = "unix:///tmp/acap-runtime.sock";
static const char* target_port = "0";
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
