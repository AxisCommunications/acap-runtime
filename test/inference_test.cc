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

#include "bitmap.h"
#include "grpcpp/create_channel.h"
#include "memory_use.h"
#include "milli_seconds.h"
#include "read_text.h"
#include "tensorflow_serving/apis/prediction_service.grpc.pb.h"
#include "testdata.h"
#include "verbose_setting.h"
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <thread>

int AcapRuntime(int argc, char* argv[]);

using namespace ::testing;
using namespace std;
using namespace grpc;
using namespace google::protobuf;
using namespace tensorflow;
using namespace tensorflow::serving;

namespace acap_runtime {
namespace inference_test {

const char* cpuChipId = "2";
const char* tpuChipId = "4";
const char* dlpuChipId = "12";

const char* sharedFile = "/test.bmp";
int logTime = -1;

void LogMemory() {
    time_t now = time(NULL);
    struct tm* nTime = localtime(&now);
    int nowTime = nTime->tm_hour;
    if (nowTime != logTime) {
        int mem = memory_use();
        printf("%d/%d %02d:%02d %d kB\n",
               nTime->tm_mday,
               nTime->tm_mon,
               nTime->tm_hour,
               nTime->tm_min,
               mem);
        logTime = nowTime;
    }
}

void Service(int seconds, const char* chipId) {
    char timeout[10];
    sprintf(timeout, "%d", seconds);
    const bool verbose = get_verbose_status();
    char const* argv[] =
        {"acapruntime", verbose ? "-v" : "", "-p", target_port, "-t", timeout, "-j", chipId};
    const int argc = sizeof(argv) / sizeof(const char*);
    ASSERT_EQ(0, AcapRuntime(argc, (char**)argv));
}

void ServiceSecurity(int seconds,
                     const char* chipId,
                     const char* certificateFile,
                     const char* keyFile) {
    char timeout[10];
    sprintf(timeout, "%d", seconds);
    const bool verbose = get_verbose_status();
    char const* argv[] = {"acapruntime",
                          verbose ? "-v" : "",
                          "-p",
                          target_port,
                          "-t",
                          timeout,
                          "-j",
                          chipId,
                          "-c",
                          certificateFile,
                          "-k",
                          keyFile};
    const int argc = sizeof(argv) / sizeof(const char*);
    ASSERT_EQ(0, AcapRuntime(argc, (char**)argv));
}

void ServiceModel(int seconds, const char* chipId, const char* modelFile) {
    char timeout[10];
    sprintf(timeout, "%d", seconds);
    const bool verbose = get_verbose_status();
    char const* argv[] = {"acapruntime",
                          verbose ? "-v" : "",
                          "-p",
                          target_port,
                          "-t",
                          timeout,
                          "-j",
                          chipId,
                          "-m",
                          modelFile};
    const int argc = sizeof(argv) / sizeof(const char*);
    ASSERT_EQ(0, AcapRuntime(argc, (char**)argv));
}

void PredictModel1(unique_ptr<PredictionService::Stub>& stub,
                   const char* modelPath,
                   const char* imageFile,
                   float score0,
                   float score1,
                   bool zeroCopy) {
    int fd = -1;
    void* data = nullptr;

    int width;
    int height;
    int channels;
    uchar* pixels;
    ReadImage(imageFile, &pixels, &width, &height, &channels);
    size_t size = width * height * channels;

    // Set image
    TensorProto proto;
    proto.mutable_tensor_shape()->add_dim()->set_size(1);
    proto.mutable_tensor_shape()->add_dim()->set_size(height);
    proto.mutable_tensor_shape()->add_dim()->set_size(width);
    proto.mutable_tensor_shape()->add_dim()->set_size(channels);

    if (zeroCopy) {
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
    } else {
        proto.set_tensor_content(pixels, size);
        proto.set_dtype(tensorflow::DataType::DT_UINT8);
    }
    free(pixels);

    // Initialize input protobuf
    PredictRequest request;
    request.mutable_model_spec()->set_name(modelPath);
    Map<string, TensorProto>& inputs = *request.mutable_inputs();
    inputs["data"] = proto;

    // Inference
    PredictResponse response;
    ClientContext context;
    uint64_t start = MilliSeconds();
    Status status = stub->Predict(&context, request, &response);
    ASSERT_TRUE(status.ok());
    uint64_t elapsed = MilliSeconds() - start;
    cout << fixed << setprecision(2) << "Inference time: " << elapsed << " ms"
         << (zeroCopy ? " Z" : "") << endl;

    // Verify result
    auto& outputs = *response.mutable_outputs();
    TensorProto boxesProto = outputs["TFLite_Detection_PostProcess"];
    TensorProto classesProto = outputs["TFLite_Detection_PostProcess:1"];
    TensorProto scoresProto = outputs["TFLite_Detection_PostProcess:2"];
    TensorProto countProto = outputs["TFLite_Detection_PostProcess:3"];
    const int boxesSize = boxesProto.tensor_content().size();
    const int classesSize = classesProto.tensor_content().size();
    const int scoresSize = scoresProto.tensor_content().size();
    const int countSize = countProto.tensor_content().size();
    const float* boxes = (const float*)boxesProto.tensor_content().data();
    const float* classes = (const float*)classesProto.tensor_content().data();
    const float* scores = (const float*)scoresProto.tensor_content().data();
    const float* count = (const float*)countProto.tensor_content().data();

    EXPECT_EQ(80 * sizeof(float), boxesSize);
    EXPECT_EQ(20 * sizeof(float), classesSize);
    EXPECT_EQ(20 * sizeof(float), scoresSize);
    EXPECT_EQ(sizeof(float), countSize);

    EXPECT_EQ(20, *count);
    EXPECT_EQ(0, classes[0]);
    EXPECT_FLOAT_EQ(score0, scores[0]);
    EXPECT_EQ(31, classes[1]);
    EXPECT_FLOAT_EQ(score1, scores[1]);

    // Cleanup
    if (fd >= 0) {
        munmap(data, size);
        close(fd);
        shm_unlink(sharedFile);
    }
}

void PredictModel2(unique_ptr<PredictionService::Stub>& stub,
                   const char* modelPath,
                   const char* imageFile,
                   int index0,
                   int score0,
                   bool zeroCopy) {
    int fd = -1;
    void* data = nullptr;

    int width;
    int height;
    int channels;
    uchar* pixels;
    ReadImage(imageFile, &pixels, &width, &height, &channels);
    size_t size = width * height * channels;

    // Set image
    TensorProto proto;
    proto.mutable_tensor_shape()->add_dim()->set_size(1);
    proto.mutable_tensor_shape()->add_dim()->set_size(height);
    proto.mutable_tensor_shape()->add_dim()->set_size(width);
    proto.mutable_tensor_shape()->add_dim()->set_size(channels);

    if (zeroCopy) {
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
    } else {
        proto.set_tensor_content(pixels, size);
        proto.set_dtype(tensorflow::DataType::DT_UINT8);
    }
    free(pixels);

    // Initialize input protobuf
    PredictRequest request;
    request.mutable_model_spec()->set_name(modelPath);
    Map<string, TensorProto>& inputs = *request.mutable_inputs();
    inputs["data"] = proto;

    // Inference
    PredictResponse response;
    ClientContext context;
    uint64_t start = MilliSeconds();
    Status status = stub->Predict(&context, request, &response);
    ASSERT_TRUE(status.ok());
    uint64_t elapsed = MilliSeconds() - start;
    cout << fixed << setprecision(2) << "Inference time: " << elapsed << " ms"
         << (zeroCopy ? " Z" : "") << endl;

    // Verify result
    // Assume only one output, but the name is different in CPU and TPU version
    auto& outputs = *response.mutable_outputs();
    for (auto& [output_name, outputProto] : outputs) {
        const uint8_t* output = (const uint8_t*)outputProto.tensor_content().data();
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

    // Cleanup
    if (fd >= 0) {
        munmap(data, size);
        close(fd);
        shm_unlink(sharedFile);
    }
}

void PredictModel3(unique_ptr<PredictionService::Stub>& stub,
                   const char* modelPath,
                   const char* imageFile,
                   int index0,
                   int score0,
                   bool zeroCopy) {
    int fd = -1;
    void* data = nullptr;

    int width;
    int height;
    int channels;
    uchar* pixels;
    ReadImage(imageFile, &pixels, &width, &height, &channels);
    size_t size = width * height * channels;

    // Set image
    TensorProto proto;
    proto.mutable_tensor_shape()->add_dim()->set_size(1);
    proto.mutable_tensor_shape()->add_dim()->set_size(height);
    proto.mutable_tensor_shape()->add_dim()->set_size(width);
    proto.mutable_tensor_shape()->add_dim()->set_size(channels);

    if (zeroCopy) {
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
    } else {
        proto.set_tensor_content(pixels, size);
        proto.set_dtype(tensorflow::DataType::DT_UINT8);
    }
    free(pixels);

    // Initialize input protobuf
    PredictRequest request;
    request.mutable_model_spec()->set_name(modelPath);
    Map<string, TensorProto>& inputs = *request.mutable_inputs();
    inputs["data"] = proto;

    // Inference
    PredictResponse response;
    ClientContext context;
    uint64_t start = MilliSeconds();
    Status status = stub->Predict(&context, request, &response);
    ASSERT_TRUE(status.ok());
    uint64_t elapsed = MilliSeconds() - start;
    cout << fixed << setprecision(2) << "Inference time: " << elapsed << " ms"
         << (zeroCopy ? " Z" : "") << endl;

    // Verify result
    auto& outputs = *response.mutable_outputs();
    TensorProto outputProto = outputs["Softmax"];
    const uint8_t* output = (const uint8_t*)outputProto.tensor_content().data();
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

    // Cleanup
    if (fd >= 0) {
        munmap(data, size);
        close(fd);
        shm_unlink(sharedFile);
    }
}

void PredictFail(unique_ptr<PredictionService::Stub>& stub, const char* modelPath) {
    int fd = -1;
    void* data = nullptr;

    int width;
    int height;
    int channels;
    uchar* pixels;
    ReadImage(imageFile1, &pixels, &width, &height, &channels);
    size_t size = width * height * channels;

    // Set image
    TensorProto proto;
    proto.mutable_tensor_shape()->add_dim()->set_size(1);
    proto.mutable_tensor_shape()->add_dim()->set_size(height);
    proto.mutable_tensor_shape()->add_dim()->set_size(width);
    proto.mutable_tensor_shape()->add_dim()->set_size(channels);
    proto.set_tensor_content(pixels, size);
    proto.set_dtype(tensorflow::DataType::DT_UINT8);
    free(pixels);

    // Initialize input protobuf
    PredictRequest request;
    request.mutable_model_spec()->set_name(modelPath);
    Map<string, TensorProto>& inputs = *request.mutable_inputs();
    inputs["data"] = proto;

    // Inference
    PredictResponse response;
    ClientContext context;
    Status status = stub->Predict(&context, request, &response);
    ASSERT_FALSE(status.ok());
}

TEST(InferenceTest, ServerAuthentication) {
    shm_unlink(sharedFile);
    thread main(ServiceSecurity, 5, cpuChipId, serverCertificatePath, serverKeyPath);

    string root_cert = read_text(serverCertificatePath);
    SslCredentialsOptions ssl_opts = {root_cert.c_str(), "", ""};
    shared_ptr<ChannelCredentials> creds = grpc::SslCredentials(ssl_opts);
    shared_ptr<Channel> channel = CreateChannel(target, creds);
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel1(stub, cpuModel1, imageFile1, 0.87890601, 0.58203125, true);
    main.join();
}

TEST(InferenceTest, DISABLED_ServerInsecureCredentials_Fail) {
    shm_unlink(sharedFile);
    thread main(ServiceSecurity, 5, cpuChipId, serverCertificatePath, serverKeyPath);

    shared_ptr<ChannelCredentials> creds = grpc::InsecureChannelCredentials();
    shared_ptr<Channel> channel = CreateChannel(target, creds);
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictFail(stub, cpuModel1);
    main.join();
}

TEST(InferenceTest, PredictCpuModel1Preload) {
    shm_unlink(sharedFile);
    thread main(ServiceModel, 5, cpuChipId, cpuModel1);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel1(stub, cpuModel1, imageFile1, 0.87890601, 0.58203125, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.87890601, 0.58203125, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.87890601, 0.58203125, true);
#ifdef __arm64__
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
#elif __arm__
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
#else
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.58203125, true);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.58203125, true);
#endif
    main.join();
}

TEST(InferenceTest, PredictCpuModel1) {
    shm_unlink(sharedFile);
    thread main(Service, 5, cpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel1(stub, cpuModel1, imageFile1, 0.87890601, 0.58203125, false);
    PredictModel1(stub, cpuModel1, imageFile1, 0.87890601, 0.58203125, false);
    PredictModel1(stub, cpuModel1, imageFile1, 0.87890601, 0.58203125, true);
#ifdef __arm64__
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, false);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
#elif __arm__
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, false);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
#else
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.58203125, false);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.58203125, true);
#endif
    main.join();
}

TEST(InferenceTest, PredictCpuModel2) {
    shm_unlink(sharedFile);
    thread main(Service, 8, cpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
#ifdef __arm64__
    PredictModel2(stub, cpuModel2, imageFile1, 653, 168, false);
    PredictModel2(stub, cpuModel2, imageFile1, 653, 168, false);
    PredictModel2(stub, cpuModel2, imageFile1, 653, 168, true);
    PredictModel2(stub, cpuModel2, imageFile2, 458, 168, true);
    PredictModel2(stub, cpuModel2, imageFile2, 458, 168, true);
#elif __arm__
    PredictModel2(stub, cpuModel2, imageFile1, 653, 168, false);
    PredictModel2(stub, cpuModel2, imageFile1, 653, 168, false);
    PredictModel2(stub, cpuModel2, imageFile1, 653, 168, true);
    PredictModel2(stub, cpuModel2, imageFile2, 458, 168, true);
    PredictModel2(stub, cpuModel2, imageFile2, 458, 168, true);
#else
    PredictModel2(stub, cpuModel2, imageFile1, 653, 165, false);
    PredictModel2(stub, cpuModel2, imageFile1, 653, 165, false);
    PredictModel2(stub, cpuModel2, imageFile1, 653, 165, true);
    PredictModel2(stub, cpuModel2, imageFile2, 458, 166, true);
    PredictModel2(stub, cpuModel2, imageFile2, 458, 166, true);
#endif
    main.join();
}

TEST(InferenceTest, PredictCpuModel3) {
    shm_unlink(sharedFile);
    thread main(Service, 15, cpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
#ifdef __arm64__
    PredictModel3(stub, cpuModel3, imageFile1, 653, 190, false);
    PredictModel3(stub, cpuModel3, imageFile1, 653, 190, false);
    PredictModel3(stub, cpuModel3, imageFile1, 653, 190, true);
    PredictModel3(stub, cpuModel3, imageFile2, 653, 200, true);
    PredictModel3(stub, cpuModel3, imageFile2, 653, 200, true);
#elif __arm__
    PredictModel3(stub, cpuModel3, imageFile1, 653, 190, false);
    PredictModel3(stub, cpuModel3, imageFile1, 653, 190, false);
    PredictModel3(stub, cpuModel3, imageFile1, 653, 190, true);
    PredictModel3(stub, cpuModel3, imageFile2, 653, 200, true);
    PredictModel3(stub, cpuModel3, imageFile2, 653, 200, true);
#else
    PredictModel3(stub, cpuModel3, imageFile1, 653, 194, false);
    PredictModel3(stub, cpuModel3, imageFile2, 653, 197, true);
#endif
    main.join();
}

TEST(InferenceTest, PredictModel_Fail) {
    const char* noModelFile = "nomodel";
    shm_unlink(sharedFile);
    thread main(Service, 5, cpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictFail(stub, noModelFile);
    main.join();
}

#ifdef __arm64__
TEST(InferenceTest, ServerAuthenticationDlpu) {
    shm_unlink(sharedFile);
    thread main(ServiceSecurity, 5, dlpuChipId, serverCertificatePath, serverKeyPath);

    string root_cert = read_text(serverCertificatePath);
    SslCredentialsOptions ssl_opts = {root_cert.c_str(), "", ""};
    shared_ptr<ChannelCredentials> creds = grpc::SslCredentials(ssl_opts);
    shared_ptr<Channel> channel = CreateChannel(target, creds);
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
    main.join();
}

TEST(InferenceTest, PredictDlpuModel1Preload) {
    shm_unlink(sharedFile);
    thread main(ServiceModel, 5, dlpuChipId, cpuModel1);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
    main.join();
}

TEST(InferenceTest, PredictDlpuModel1) {
    shm_unlink(sharedFile);
    thread main(Service, 5, dlpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, false);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, false);
    PredictModel1(stub, cpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
    PredictModel1(stub, cpuModel1, imageFile2, 0.83984375, 0.5, true);
    main.join();
}

TEST(InferenceTest, PredictDlpuModel2) {
    shm_unlink(sharedFile);
    thread main(Service, 5, dlpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel2(stub, cpuModel2, imageFile1, 653, 166, false);
    PredictModel2(stub, cpuModel2, imageFile1, 653, 166, false);
    PredictModel2(stub, cpuModel2, imageFile1, 653, 166, true);
    PredictModel2(stub, cpuModel2, imageFile2, 458, 170, true);
    PredictModel2(stub, cpuModel2, imageFile2, 458, 170, true);
    main.join();
}

TEST(InferenceTest, DISABLED_PredictDlpuModel3)
// Failed to load model efficientnet-edgetpu-M_quant.tflite (Could not send message: Connection
// reset by peer)
{
    shm_unlink(sharedFile);
    thread main(Service, 10, dlpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(10, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel3(stub, cpuModel3, imageFile1, 653, 197, false);
    PredictModel3(stub, cpuModel3, imageFile1, 653, 197, false);
    PredictModel3(stub, cpuModel3, imageFile1, 653, 197, true);
    PredictModel3(stub, cpuModel3, imageFile2, 653, 176, true);
    PredictModel3(stub, cpuModel3, imageFile2, 653, 176, true);
    main.join();
}
#elif __arm__
TEST(InferenceTest, ServerAuthenticationTpu) {
    shm_unlink(sharedFile);
    thread main(ServiceSecurity, 5, tpuChipId, serverCertificatePath, serverKeyPath);

    string root_cert = read_text(serverCertificatePath);
    SslCredentialsOptions ssl_opts = {root_cert.c_str(), "", ""};
    shared_ptr<ChannelCredentials> creds = grpc::SslCredentials(ssl_opts);
    shared_ptr<Channel> channel = CreateChannel(target, creds);
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile2, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile2, 0.878906, 0.5, true);
    main.join();
}

TEST(InferenceTest, PredictTpuModel1Preload) {
    shm_unlink(sharedFile);
    thread main(ServiceModel, 5, tpuChipId, tpuModel1);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile2, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile2, 0.878906, 0.5, true);
    main.join();
}

TEST(InferenceTest, PredictTpuModel1) {
    shm_unlink(sharedFile);
    thread main(Service, 5, tpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, false);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, false);
    PredictModel1(stub, tpuModel1, imageFile1, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile2, 0.878906, 0.5, true);
    PredictModel1(stub, tpuModel1, imageFile2, 0.878906, 0.5, true);
    main.join();
}

TEST(InferenceTest, PredictTpuModel2) {
    shm_unlink(sharedFile);
    thread main(Service, 5, tpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel2(stub, tpuModel2, imageFile1, 653, 118, false);
    PredictModel2(stub, tpuModel2, imageFile1, 653, 118, false);
    PredictModel2(stub, tpuModel2, imageFile1, 653, 118, true);
    PredictModel2(stub, tpuModel2, imageFile2, 458, 69, true);
    PredictModel2(stub, tpuModel2, imageFile2, 458, 69, true);
    main.join();
}

TEST(InferenceTest, PredictTpuModel3) {
    shm_unlink(sharedFile);
    thread main(Service, 5, tpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    PredictModel3(stub, tpuModel3, imageFile1, 653, 197, false);
    PredictModel3(stub, tpuModel3, imageFile1, 653, 197, false);
    PredictModel3(stub, tpuModel3, imageFile1, 653, 197, true);
    PredictModel3(stub, tpuModel3, imageFile2, 653, 176, true);
    PredictModel3(stub, tpuModel3, imageFile2, 653, 176, true);
    main.join();
}
#endif

TEST(InferenceTest, DISABLED_PredictLoop) {
    shm_unlink(sharedFile);
    thread main(Service, 24 * 3600, cpuChipId);

    shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
    ASSERT_TRUE(channel->WaitForConnected(
        gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(10, GPR_TIMESPAN))));
    unique_ptr<PredictionService::Stub> stub = PredictionService::NewStub(channel);
    while (channel->GetState(false) == grpc_connectivity_state::GRPC_CHANNEL_READY) {
        LogMemory();
        PredictModel1(stub, cpuModel1, imageFile1, 0.87890601, 0.58203125, true);
    }

    main.join();
}

}  // namespace inference_test
}  // namespace acap_runtime
