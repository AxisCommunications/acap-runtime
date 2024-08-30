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

#include "prediction_service.grpc.pb.h"
#include "video_capture.h"
#include <larod.h>

namespace acap_runtime {
class Inference : public tensorflow::serving::PredictionService::Service {
  public:
    using ModelSpec = tensorflow::serving::ModelSpec;
    using PredictRequest = tensorflow::serving::PredictRequest;
    using PredictResponse = tensorflow::serving::PredictResponse;
    using ServerContext = grpc::ServerContext;
    using Status = grpc::Status;
    using TensorProto = tensorflow::TensorProto;

    Inference();
    ~Inference();
    bool Init(const bool verbose,
              const uint64_t chipId,
              const std::vector<std::string>& models,
              Capture* captureService);
    Status Predict(ServerContext* context,
                   const PredictRequest* request,
                   PredictResponse* response) override;

  private:
    void PrintError(const char* msg, larodError* error);
    void PrintErrorWithErrno(const char* msg);
    void PrintTensorProtoDebug(const TensorProto& tp);
    void PrintTensorInfo(larodTensor** tensors, size_t numTensors);
    bool CreateTmpFile(FILE*& file, int& fd, const void* data, const size_t data_size);
    void CloseTmpFile(FILE*& file, const int& fd);
    void CloseTmpFiles(std::vector<std::pair<FILE*, int>>& tmpFiles);
    bool LoadModel(larodConnection& conn,
                   const char* modelFile,
                   const larodChip chip,
                   const larodAccess access);
    bool SetupPreprocessing(TensorProto tp,
                            larodTensor* tensor,
                            std::vector<std::pair<FILE*, int>>& inFiles,
                            const u_int32_t stream,
                            uint32_t& frame_ref,
                            larodError*& error);
    bool SetupInputTensors(larodModel*& model,
                           const google::protobuf::Map<std::string, TensorProto>& inputs,
                           std::vector<std::pair<FILE*, int>>& inFiles,
                           const u_int32_t stream,
                           uint32_t& frame_ref,
                           larodError*& error);
    bool SetupOutputTensors(larodModel*& model,
                            std::vector<std::pair<FILE*, int>>& outFiles,
                            larodError*& error);
    bool LarodOutputToPredictResponse(PredictResponse*& response,
                                      const ModelSpec& model_spec,
                                      larodModel*& model,
                                      std::vector<std::pair<FILE*, int>>& outFiles,
                                      larodError*& error);

    bool _verbose;
    larodConnection* _conn;
    larodChip _chipId;
    std::map<std::string, larodModel*> _models;
    larodModel* _ppModel;
    larodMap* _ppMap;
    pthread_mutex_t _mtx;
    larodTensor** _ppInputTensors;
    larodTensor** _ppOutputTensors;
    larodTensor** _inputTensors;
    larodTensor** _outputTensors;
    size_t _ppNumInputs;
    size_t _ppNumOutputs;
    size_t _numInputs;
    size_t _numOutputs;
    Capture* _captureService;
};
}  // namespace acap_runtime
