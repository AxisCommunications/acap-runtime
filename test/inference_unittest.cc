/* Copyright 2020 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <gtest/gtest.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "milli_seconds.h"
#include "inference.h"
#include "bitmap.h"
#include "testdata.h"

using namespace ::testing;
using namespace std;
using namespace grpc;
using namespace tensorflow;
using namespace tensorflow::serving;
using namespace google::protobuf;

namespace acap_runtime {
namespace inference_unittest {

const uint64_t cpuChipId = 2;
const uint64_t tpuChipId = 4;
const uint64_t dlpuChipId = 12;
const char* sharedFile = "/test.bmp";
Capture capture;

void PredictModel1(
  Inference& inference,
  string modelName,
  const char* imageFile,
  float score0,
  float score1,
  bool zeroCopy)
{
  const float CONFIDENCE_CUTOFF = 0.3;
  int fd = -1;
  void* data = nullptr;

  uchar *pixels;
  int width;
  int height;
  int channels;
  ReadImage(imageFile, &pixels, &width, &height, &channels);
  size_t size = width * height * channels;

  // Set image, LAROD_TENSOR_LAYOUT_NHWC assumed
  TensorProto proto;
  proto.mutable_tensor_shape()->add_dim()->set_size(1);
  proto.mutable_tensor_shape()->add_dim()->set_size(height);
  proto.mutable_tensor_shape()->add_dim()->set_size(width);
  proto.mutable_tensor_shape()->add_dim()->set_size(channels);

  if (zeroCopy)
  {
    // Create memory mapped file
    fd = shm_open(sharedFile, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(ftruncate(fd, size), 0);

    // Get an address to fd's memory for this process's memory space
    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ASSERT_NE(data, MAP_FAILED);
    memcpy(data, pixels, size);
    proto.add_string_val(sharedFile);
    proto.set_dtype(tensorflow::DataType::DT_STRING);
  }
  else
  {
    proto.set_tensor_content(pixels, size);
    proto.set_dtype(DataType::DT_UINT8);
  }
  free(pixels);

  // Initialize input protobuf
  PredictRequest request;
  request.mutable_model_spec()->set_name(modelName);
  Map<string, TensorProto> &inputs = *request.mutable_inputs();
  inputs["data"] = proto;

  // Inference
  PredictResponse response;
  ServerContext context;
  uint64_t start = MilliSeconds();
  Status status = inference.Predict(&context, &request, &response);
  ASSERT_TRUE(status.ok());
  uint64_t elapsed = MilliSeconds() - start;
  cout << fixed << setprecision(2) << "Inference time: "
    << elapsed << " ms" << (zeroCopy ? " Z" : "") << endl;

  // Verify result
  auto &outputs = *response.mutable_outputs();
  TensorProto boxesProto = outputs["TFLite_Detection_PostProcess"];
  TensorProto classesProto = outputs["TFLite_Detection_PostProcess:1"];
  TensorProto scoresProto = outputs["TFLite_Detection_PostProcess:2"];
  TensorProto countProto = outputs["TFLite_Detection_PostProcess:3"];
  const int boxesSize = boxesProto.tensor_content().size();
  const int classesSize = classesProto.tensor_content().size();
  const int scoresSize = scoresProto.tensor_content().size();
  const int countSize = countProto.tensor_content().size();
  const float *boxes = (const float *)boxesProto.tensor_content().data();
  const float *classes = (const float *)classesProto.tensor_content().data();
  const float *scores = (const float *)scoresProto.tensor_content().data();
  const float *count = (const float *)countProto.tensor_content().data();

  EXPECT_EQ(80 * sizeof(float), boxesSize);
  EXPECT_EQ(20 * sizeof(float), classesSize);
  EXPECT_EQ(20 * sizeof(float), scoresSize);
  EXPECT_EQ(sizeof(float), countSize);

  EXPECT_EQ(20, *count);
  EXPECT_EQ(0, classes[0]);
  EXPECT_FLOAT_EQ(score0, scores[0]);
  EXPECT_EQ(31, classes[1]);
  EXPECT_FLOAT_EQ(score1, scores[1]);

  if (fd >= 0) {
    munmap(data, size);
    shm_unlink(sharedFile);
    close (fd);
  }
}

void PredictModel2(
  Inference& inference,
  string modelName,
  const char* imageFile,
  int index0,
  int score0,
  bool zeroCopy)
{
  const float CONFIDENCE_CUTOFF = 0.3;
  int fd = -1;
  void* data = nullptr;

  uchar *pixels;
  int width;
  int height;
  int channels;
  ReadImage(imageFile, &pixels, &width, &height, &channels);
  size_t size = width * height * channels;

  // Set image, LAROD_TENSOR_LAYOUT_NHWC assumed
  TensorProto proto;
  proto.mutable_tensor_shape()->add_dim()->set_size(1);
  proto.mutable_tensor_shape()->add_dim()->set_size(height);
  proto.mutable_tensor_shape()->add_dim()->set_size(width);
  proto.mutable_tensor_shape()->add_dim()->set_size(channels);

  if (zeroCopy)
  {
    // Create memory mapped file
    fd = shm_open(sharedFile, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(ftruncate(fd, size), 0);

    // Get an address to fd's memory for this process's memory space
    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ASSERT_NE(data, MAP_FAILED);
    memcpy(data, pixels, size);
    proto.add_string_val(sharedFile);
    proto.set_dtype(tensorflow::DataType::DT_STRING);
  }
  else
  {
    proto.set_tensor_content(pixels, size);
    proto.set_dtype(DataType::DT_UINT8);
  }
  free(pixels);

  // Initialize input protobuf
  PredictRequest request;
  request.mutable_model_spec()->set_name(modelName);
  Map<string, TensorProto> &inputs = *request.mutable_inputs();
  inputs["data"] = proto;

  // Inference
  PredictResponse response;
  ServerContext context;
  uint64_t start = MilliSeconds();
  Status status = inference.Predict(&context, &request, &response);
  ASSERT_TRUE(status.ok());
  uint64_t elapsed = MilliSeconds() - start;
  cout << fixed << setprecision(2) << "Inference time: "
    << elapsed << " ms" << (zeroCopy ? " Z" : "") << endl;

  // Verify result
  // Assume only one output, but the name is different in CPU and TPU version
  auto &outputs = *response.mutable_outputs();
  for (auto & [output_name, outputProto] : outputs) {
    const uint8_t *output =  (const uint8_t *)outputProto.tensor_content().data();
    size_t count = outputProto.tensor_content().size();
    EXPECT_EQ(1001, count);

  // Compute the most likely index.
    uint8_t maxProb = 0;
    size_t maxIdx = 0;
    for (size_t i = 0; i < count; i++) {
      uint8_t value = output[i];
        if (value > maxProb) {
            maxProb = value;
            maxIdx = i;
        }
    }

    EXPECT_EQ(index0, maxIdx);
    EXPECT_FLOAT_EQ(score0, maxProb);
  }

  if (fd >= 0) {
    munmap(data, size);
    shm_unlink(sharedFile);
    close (fd);
  }
}

void PredictModel3(
  Inference& inference,
  string modelName,
  const char* imageFile,
  int index0,
  int score0,
  bool zeroCopy)
{
  const float CONFIDENCE_CUTOFF = 0.3;
  int fd = -1;
  void* data = nullptr;

  uchar *pixels;
  int width;
  int height;
  int channels;
  ReadImage(imageFile, &pixels, &width, &height, &channels);
  size_t size = width * height * channels;

  // Set image, LAROD_TENSOR_LAYOUT_NHWC assumed
  TensorProto proto;
  proto.mutable_tensor_shape()->add_dim()->set_size(1);
  proto.mutable_tensor_shape()->add_dim()->set_size(height);
  proto.mutable_tensor_shape()->add_dim()->set_size(width);
  proto.mutable_tensor_shape()->add_dim()->set_size(channels);

  if (zeroCopy)
  {
    // Create memory mapped file
    fd = shm_open(sharedFile, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(ftruncate(fd, size), 0);

    // Get an address to fd's memory for this process's memory space
    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ASSERT_NE(data, MAP_FAILED);
    memcpy(data, pixels, size);
    proto.add_string_val(sharedFile);
    proto.set_dtype(tensorflow::DataType::DT_STRING);
  }
  else
  {
    proto.set_tensor_content(pixels, size);
    proto.set_dtype(DataType::DT_UINT8);
  }
  free(pixels);

  // Initialize input protobuf
  PredictRequest request;
  request.mutable_model_spec()->set_name(modelName);
  Map<string, TensorProto> &inputs = *request.mutable_inputs();
  inputs["data"] = proto;

  // Inference
  PredictResponse response;
  ServerContext context;
  uint64_t start = MilliSeconds();
  Status status = inference.Predict(&context, &request, &response);
  ASSERT_TRUE(status.ok());
  uint64_t elapsed = MilliSeconds() - start;
  cout << fixed << setprecision(2) << "Inference time: "
    << elapsed << " ms" << (zeroCopy ? " Z" : "") << endl;

  // Verify result
  auto &outputs = *response.mutable_outputs();
  TensorProto outputProto = outputs["Softmax"];
  const uint8_t *output =  (const uint8_t *)outputProto.tensor_content().data();
  size_t count = outputProto.tensor_content().size();
  EXPECT_EQ(1001, count);

// Compute the most likely index.
  uint8_t maxProb = 0;
  size_t maxIdx = 0;
  for (size_t i = 0; i < count; i++) {
    uint8_t value = output[i];
      if (value > maxProb) {
          maxProb = value;
          maxIdx = i;
      }
  }

  EXPECT_EQ(index0, maxIdx);
  EXPECT_FLOAT_EQ(score0, maxProb);

  if (fd >= 0) {
    munmap(data, size);
    shm_unlink(sharedFile);
    close (fd);
  }
}

TEST(InferenceUnittest, InitCpu)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  if (verbose) {
#ifdef __arm32__
    cout << "Defined: __arm32__" << endl;
#endif
#ifdef __arm64__
    cout << "Defined: __arm64__" << endl;
#endif
#ifdef __arm__
    cout << "Defined: __arm__" << endl;
#endif
#ifdef __amd64__
    cout << "Defined: __amd64__" << endl;
#endif
#ifdef __thumb__
    cout << "Defined: __thumb__" << endl;
#endif
#ifdef __ARM_ARCH_5__
    cout << "Defined: __ARM_ARCH_5__" << endl;
#endif
#ifdef __ARM_ARCH_6__
    cout << "Defined: __ARM_ARCH_6__" << endl;
#endif
#ifdef __ARM_ARCH_7__
    cout << "Defined: __ARM_ARCH_7__" << endl;
#endif
#ifdef __ARM_ARCH_8__
    cout << "Defined: __ARM_ARCH_8__" << endl;
#endif
  }

  const vector<string> models = { cpuModel1 };
  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, cpuChipId, models, &capture));
}

TEST(InferenceUnittest, Init_Fail)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { cpuModel1, "invalid" };

  Inference inference;
  ASSERT_FALSE(inference.Init(verbose, cpuChipId, models, &capture));
}

TEST(InferenceUnittest, PredictCpuModel1Preload)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { cpuModel1 };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, cpuChipId, models, &capture));
  PredictModel1(inference, cpuModel1, imageFile1, 0.87890601, 0.58203125, true);
  PredictModel1(inference, cpuModel1, imageFile1, 0.87890601, 0.58203125, true);
  PredictModel1(inference, cpuModel1, imageFile1, 0.87890601, 0.58203125, true);
#ifdef __arm64__
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
#elif __arm__
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
#else
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.58203125, true);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.58203125, true);
#endif
}

TEST(InferenceUnittest, PredictCpuModel1)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, cpuChipId, models, &capture));
  PredictModel1(inference, cpuModel1, imageFile1, 0.87890601, 0.58203125, false);
  PredictModel1(inference, cpuModel1, imageFile1, 0.87890601, 0.58203125, false);
  PredictModel1(inference, cpuModel1, imageFile1, 0.87890601, 0.58203125, true);
#ifdef __arm64__
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
#elif __arm__
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
#else
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.58203125, true);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.58203125, true);
#endif
}

TEST(InferenceUnittest, PredictCpuModel2)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, cpuChipId, models, &capture));
#ifdef __arm64__
  PredictModel2(inference, cpuModel2, imageFile1, 653, 168, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 168, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 168, true);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 168, true);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 168, true);
#elif __arm__
  PredictModel2(inference, cpuModel2, imageFile1, 653, 168, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 168, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 168, true);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 168, true);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 168, true);
#else
  PredictModel2(inference, cpuModel2, imageFile1, 653, 165, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 165, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 165, true);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 166, true);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 166, true);
#endif
}

TEST(InferenceUnittest, PredictCpuModel3)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, cpuChipId, models, &capture));
#ifdef __arm64__
  PredictModel3(inference, cpuModel3, imageFile1, 653, 190, false);
  PredictModel3(inference, cpuModel3, imageFile1, 653, 190, false);
  PredictModel3(inference, cpuModel3, imageFile1, 653, 190, true);
  PredictModel3(inference, cpuModel3, imageFile2, 653, 200, true);
  PredictModel3(inference, cpuModel3, imageFile2, 653, 200, true);
#elif __arm__
  PredictModel3(inference, cpuModel3, imageFile1, 653, 190, false);
  PredictModel3(inference, cpuModel3, imageFile1, 653, 190, false);
  PredictModel3(inference, cpuModel3, imageFile1, 653, 190, true);
  PredictModel3(inference, cpuModel3, imageFile2, 653, 200, true);
  PredictModel3(inference, cpuModel3, imageFile2, 653, 200, true);
#else
  PredictModel3(inference, cpuModel3, imageFile1, 653, 194, false);
  PredictModel3(inference, cpuModel3, imageFile1, 653, 194, false);
  PredictModel3(inference, cpuModel3, imageFile1, 653, 194, true);
  PredictModel3(inference, cpuModel3, imageFile2, 653, 197, true);
  PredictModel3(inference, cpuModel3, imageFile2, 653, 197, true);
#endif
}

TEST(InferenceUnittest, PredictCpuModelMix)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, cpuChipId, models, &capture));
#ifdef __arm64__
  PredictModel1(inference, cpuModel1, imageFile1, 0.87890601, 0.58203125, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 168, false);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 168, true);
  PredictModel3(inference, cpuModel3, imageFile1, 653, 190, false);
  PredictModel3(inference, cpuModel3, imageFile2, 653, 200, false);
#elif __arm__
  PredictModel1(inference, cpuModel1, imageFile1, 0.87890601, 0.58203125, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 168, false);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 168, true);
  PredictModel3(inference, cpuModel3, imageFile1, 653, 190, false);
  PredictModel3(inference, cpuModel3, imageFile2, 653, 200, false);
#else
  PredictModel1(inference, cpuModel1, imageFile1, 0.87890601, 0.58203125, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 165, false);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.58203125, true);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 166, true);
#endif
}

#ifdef __arm64__
TEST(InferenceUnittest, InitDlpu)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { cpuModel1 };

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, dlpuChipId, models));
}

TEST(InferenceUnittest, PredictDlpuModel1Preload)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { cpuModel1 };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, dlpuChipId, models));
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
}

TEST(InferenceUnittest, PredictDlpuModel1)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, dlpuChipId, models));
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, false);
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, false);
  PredictModel1(inference, cpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
  PredictModel1(inference, cpuModel1, imageFile2, 0.83984375, 0.5, true);
}

TEST(InferenceUnittest, PredictDlpuModel2)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, dlpuChipId, models));
  PredictModel2(inference, cpuModel2, imageFile1, 653, 166, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 166, false);
  PredictModel2(inference, cpuModel2, imageFile1, 653, 166, false);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 170, true);
  PredictModel2(inference, cpuModel2, imageFile2, 458, 170, true);
}

TEST(InferenceUnittest, DISABLED_PredictDlpuModel3)
// ERROR in Inference: Failed to load model efficientnet-edgetpu-M_quant.tflite
// (Could not load model: Asynchronous connection has been closed)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, dlpuChipId, models));
  PredictModel3(inference, cpuModel3, imageFile1, 653, 197, false);
  PredictModel3(inference, cpuModel3, imageFile1, 653, 197, false);
  PredictModel3(inference, cpuModel3, imageFile1, 653, 197, false);
  PredictModel3(inference, cpuModel3, imageFile2, 653, 176, false);
  PredictModel3(inference, cpuModel3, imageFile2, 653, 176, false);
}
#elif __arm__
TEST(InferenceUnittest, InitTpu)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { tpuModel1 };

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, tpuChipId, models, &capture));
}

TEST(InferenceUnittest, PredictTpuModel1Preload)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { tpuModel1 };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, tpuChipId, models, &capture));
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile2, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile2, 0.878906, 0.5, true);
}

TEST(InferenceUnittest, PredictTpuModel1)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, tpuChipId, models, &capture));
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, false);
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, false);
  PredictModel1(inference, tpuModel1, imageFile1, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile2, 0.878906, 0.5, true);
  PredictModel1(inference, tpuModel1, imageFile2, 0.878906, 0.5, true);
}

TEST(InferenceUnittest, PredictTpuModel2)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, tpuChipId, models, &capture));
  PredictModel2(inference, tpuModel2, imageFile1, 653, 118, false);
  PredictModel2(inference, tpuModel2, imageFile1, 653, 118, false);
  PredictModel2(inference, tpuModel2, imageFile1, 653, 118, false);
  PredictModel2(inference, tpuModel2, imageFile2, 458, 69, true);
  PredictModel2(inference, tpuModel2, imageFile2, 458, 69, true);
}

TEST(InferenceUnittest, PredictTpuModel3)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  const vector<string> models = { };
  shm_unlink(sharedFile);

  Inference inference;
  ASSERT_TRUE(inference.Init(verbose, tpuChipId, models, &capture));
  PredictModel3(inference, tpuModel3, imageFile1, 653, 197, false);
  PredictModel3(inference, tpuModel3, imageFile1, 653, 197, false);
  PredictModel3(inference, tpuModel3, imageFile1, 653, 197, false);
  PredictModel3(inference, tpuModel3, imageFile2, 653, 176, false);
  PredictModel3(inference, tpuModel3, imageFile2, 653, 176, false);
}
#endif

}  // namespace inference_unittest
}  // namespace acap_runtime