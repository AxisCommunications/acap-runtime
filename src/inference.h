/* Copyright 2020 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#pragma once
#include <larod.h>
#include "prediction_service.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace tensorflow;
using namespace tensorflow::serving;

namespace inference_server {

class Inference : public PredictionService::Service
{
public:
  Inference();
  ~Inference();
  bool Init(
    const bool verbose,
    const uint64_t chipId,
    const vector<string>& models);
  Status Predict(
    ServerContext* context,
    const PredictRequest* request,
    PredictResponse* response) override;

private:
  void PrintError(const char* msg, larodError* error);
  void PrintErrorWithErrno(const char* msg);
  void PrintTensorProtoDebug(const TensorProto& tp);
  void PrintTensorInfo(larodTensor** tensors, size_t numTensors);
  bool CreateTmpFile(FILE*& file, int& fd, const void* data, const size_t data_size);
  void CloseTmpFile(FILE*& file, const int& fd);
  void CloseTmpFiles(vector<pair<FILE*, int>>& tmpFiles);
  bool LoadModel(
    larodConnection& conn,
    const char* modelFile,
    const larodChip chip,
    const larodAccess access);
  bool SetupPreprocessing(
    TensorProto tp,
    larodTensor* tensor,
    vector<pair<FILE*, int>>& inFiles,
    larodError*& error);
  bool SetupInputTensors(
    larodModel*& model,
    const google::protobuf::Map<string, TensorProto>& inputs,
    vector<pair<FILE*, int>>& inFiles,
    larodError*& error);
  bool SetupOutputTensors(
    larodModel*& model,
    vector<pair<FILE*, int>>& outFiles,
    larodError*& error);
  bool LarodOutputToPredictResponse(
    PredictResponse*& response,
    const ModelSpec& model_spec,
    larodModel*& model,
    vector<pair<FILE*, int>>& outFiles,
    larodError*& error);

  bool _verbose;
  larodConnection* _conn;
  larodChip _chipId;
  map<string, larodModel*> _models;
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
};

}  // namespace inference_server
