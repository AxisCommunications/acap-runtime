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

#include "inference.h"
#include <chrono>
#include <fcntl.h>
#include <grpcpp/grpcpp.h>
#include <iomanip>
#include <sstream>
#include <sys/mman.h>
#include <unistd.h>

#define ERRORLOG std::cerr << "ERROR in Inference: "
#define TRACELOG  \
    if (_verbose) \
    std::cout << "TRACE in Inference: "

using namespace std::chrono;

namespace acap_runtime {
const char* const DATA_TYPES[] = {"LAROD_TENSOR_DATA_TYPE_INVALID",
                                  "LAROD_TENSOR_DATA_TYPE_UNSPECIFIED",
                                  "LAROD_TENSOR_DATA_TYPE_BOOL",
                                  "LAROD_TENSOR_DATA_TYPE_UINT8",
                                  "LAROD_TENSOR_DATA_TYPE_INT8",
                                  "LAROD_TENSOR_DATA_TYPE_UINT16",
                                  "LAROD_TENSOR_DATA_TYPE_INT16",
                                  "LAROD_TENSOR_DATA_TYPE_UINT32",
                                  "LAROD_TENSOR_DATA_TYPE_INT32",
                                  "LAROD_TENSOR_DATA_TYPE_UINT64",
                                  "LAROD_TENSOR_DATA_TYPE_INT64",
                                  "LAROD_TENSOR_DATA_TYPE_FLOAT16",
                                  "LAROD_TENSOR_DATA_TYPE_FLOAT32",
                                  "LAROD_TENSOR_DATA_TYPE_FLOAT64"};
const char* const LAYOUTS[] = {"LAROD_TENSOR_LAYOUT_INVALID",
                               "LAROD_TENSOR_LAYOUT_UNSPECIFIED",
                               "LAROD_TENSOR_LAYOUT_NHWC",
                               "LAROD_TENSOR_LAYOUT_NCHW",
                               "LAROD_TENSOR_LAYOUT_420SP"};

Inference::Inference() : _conn(nullptr), _chipId(LAROD_CHIP_INVALID), _verbose(false) {}

Inference::~Inference() {
    if (nullptr != _conn) {
        // Delete models
        TRACELOG << "Deleting loaded models:" << endl;
        larodError* error = nullptr;
        for (auto& [model_name, model] : _models) {
            TRACELOG << "- " << model_name << endl;
            if (!larodDeleteModel(_conn, model, &error)) {
                PrintError("Failed to delete model", error);
                larodClearError(&error);
            }
        }

        // Disconnect from larod service
        TRACELOG << "Disconnecting from larod" << endl;
        if (!larodDisconnect(&_conn, &error)) {
            PrintError("Failed to disconnect", error);
            larodClearError(&error);
        }
    }

    for (auto& [model_name, model] : _models) {
        larodDestroyModel(&model);
    }
}

// Initialize inference
bool Inference::Init(const bool verbose,
                     const uint64_t chipId,
                     const vector<string>& models,
                     Capture* captureService) {
    _verbose = verbose;
    larodError* error = nullptr;

    _captureService = captureService;

    TRACELOG << "Init chipId=" << chipId << endl;

    if (pthread_mutex_init(&_mtx, NULL) != 0) {
        ERRORLOG << "Init mutex FAILED" << endl;
        return false;
    }

    // Connect to larod service
    if (!larodConnect(&_conn, &error)) {
        PrintError("Connecting to larod FAILED", error);
        larodClearError(&error);
        return false;
    }

    // List available chip id:s
    larodChip* chipIds = nullptr;
    size_t numChipIds = 0;
    if (larodListChips(_conn, &chipIds, &numChipIds, &error)) {
        TRACELOG << "Available chip ids:" << endl;
        for (size_t i = 0; i < numChipIds; ++i) {
            TRACELOG << chipIds[i] << ": " << larodGetChipName(chipIds[i]) << endl;
        }
        free(chipIds);
    } else {
        PrintError("Failed to list available chip id:s", error);
        larodClearError(&error);
    }

    // Show selected chip
    _chipId = static_cast<larodChip>(chipId);
    TRACELOG << "Selected chip for this session: " << larodGetChipName(_chipId) << endl;

    // Load models if any
    _models.clear();
    for (auto model : models) {
        if (!LoadModel(*_conn, model.c_str(), _chipId, LAROD_ACCESS_PRIVATE)) {
            return false;
        }
    }

    return true;
}

// Run inference on a single image
Status Inference::Predict(ServerContext* context,
                          const PredictRequest* request,
                          PredictResponse* response) {
    auto status = Status::CANCELLED;
    bool ret;
    larodError* error = nullptr;
    larodJobRequest* ppJobReq;
    larodJobRequest* jobReq;
    uint64_t totalTime;
    uint64_t larodTime;
    vector<pair<FILE*, int>> inFiles;
    vector<pair<FILE*, int>> outFiles;
    uint32_t frame_ref;
    (void)context;

    // Validate parameters
    if (nullptr == _conn) {
        ERRORLOG << "No valid larod connection" << endl;
        return Status::CANCELLED;
    }
    if (nullptr == request) {
        ERRORLOG << "Unexpected NULL request in parameter" << endl;
        return Status::CANCELLED;
    }
    if (nullptr == response) {
        ERRORLOG << "Unexpected NULL response in parameter" << endl;
        return Status::CANCELLED;
    }

    // Print request info and start timing
    if (_verbose) {
        totalTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        TRACELOG << "Incoming request:" << request->model_spec().DebugString();
    }

    // Find model
    string modelName = request->model_spec().name();
    auto model_it = _models.find(modelName);
    if (_models.end() == model_it) {
        // Try to load model from file
        TRACELOG << "Loading model file " << modelName << endl;
        if (!LoadModel(*_conn, modelName.c_str(), _chipId, LAROD_ACCESS_PRIVATE)) {
            return Status::CANCELLED;
        }

        model_it = _models.find(modelName);
        if (_models.end() == model_it) {
            ERRORLOG << "Unknown model " << modelName << endl;
            return Status::CANCELLED;
        }
    }
    auto& model_name = model_it->first;
    auto& model = model_it->second;

    // Make larod calls atomic and threadsafe
    pthread_mutex_lock(&_mtx);

    // Clear class data
    _ppModel = nullptr;
    _ppMap = nullptr;
    _ppInputTensors = nullptr;
    _ppOutputTensors = nullptr;
    _inputTensors = nullptr;
    _outputTensors = nullptr;
    _ppNumInputs = 0;
    _ppNumOutputs = 0;
    _numInputs = 0;
    _numOutputs = 0;

    // Setup input tensors
    if (!SetupInputTensors(model,
                           request->inputs(),
                           inFiles,
                           request->stream_id(),
                           frame_ref,
                           error)) {
        goto predict_error;
    }

    response->set_frame_reference(frame_ref);

    // Setup output tensors
    if (!SetupOutputTensors(model, outFiles, error)) {
        goto predict_error;
    }

    if (_verbose) {
        TRACELOG << "Preprocessing input tensors:" << endl;
        PrintTensorInfo(_ppInputTensors, _ppNumInputs);
        TRACELOG << "Preprocessing output tensors:" << endl;
        PrintTensorInfo(_ppOutputTensors, _ppNumOutputs);
        TRACELOG << "inference input tensors:" << endl;
        PrintTensorInfo(_inputTensors, _numInputs);
        TRACELOG << "Inference output tensors:" << endl;
        PrintTensorInfo(_outputTensors, _numOutputs);
        larodTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    // Run preprocessing if needed
    if (_ppNumInputs > 0) {
        TRACELOG << "Creating preprocessing request for model " << model_name << endl;
        ppJobReq = larodCreateJobRequest(_ppModel,
                                         _ppInputTensors,
                                         _ppNumInputs,
                                         _ppOutputTensors,
                                         _ppNumOutputs,
                                         nullptr,
                                         &error);
        if (!ppJobReq) {
            PrintError("Failed creating preprocessing job request", error);
            goto predict_error;
        }
        ret = larodRunJob(_conn, ppJobReq, &error);
        larodDestroyJobRequest(&ppJobReq);
        if (!ret) {
            PrintError("Preprocessing request failed", error);
            goto predict_error;
        }
    }

    // Request inference from larod
    TRACELOG << "Creating inference request for model " << model_name << endl;
    jobReq = larodCreateJobRequest(model,
                                   _inputTensors,
                                   _numInputs,
                                   _outputTensors,
                                   _numOutputs,
                                   nullptr,  // No params used.
                                   &error);
    if (nullptr == jobReq) {
        PrintError("Failed to create inference request", error);
        goto predict_error;
    }
    ret = larodRunJob(_conn, jobReq, &error);
    larodDestroyJobRequest(&jobReq);
    if (!ret) {
        PrintError("Inference request failed", error);
        goto predict_error;
    }

    if (_verbose) {
        larodTime =
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() - larodTime;
    }

    // Store Larod result in response
    if (!LarodOutputToPredictResponse(response, request->model_spec(), model, outFiles, error)) {
        goto predict_error;
    }

    // Print inference time
    if (_verbose) {
        totalTime =
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() - totalTime;
        auto overheadTime = totalTime - larodTime;
        TRACELOG << fixed << setprecision(2) << "Total time for the request: " << totalTime
                 << " ms (=> max throughput " << (double)1000 / totalTime << " FPS)" << endl;
        TRACELOG << "Time for the call to larod: " << larodTime << " ms (=> max throughput "
                 << (double)1000 / larodTime << " FPS)" << endl;
        TRACELOG << "Inference server overhead:  " << overheadTime << " ms ("
                 << (double)(100 * overheadTime) / totalTime << "%)" << endl;
    }
    status = Status::OK;

predict_error:
    // Cleanup
    larodDestroyTensors(&_ppInputTensors, _ppNumInputs);
    larodDestroyTensors(&_ppOutputTensors, _ppNumOutputs);
    larodDestroyTensors(&_inputTensors, _numInputs);
    larodDestroyTensors(&_outputTensors, _numOutputs);
    larodDestroyMap(&_ppMap);
    larodDeleteModel(_conn, _ppModel, &error);
    larodDestroyModel(&_ppModel);
    CloseTmpFiles(inFiles);
    CloseTmpFiles(outFiles);
    pthread_mutex_unlock(&_mtx);
    larodClearError(&error);
    return status;
}

// Convert from TensorFlow to larod datatype
inline const larodTensorDataType TfToLarodDataType(const DataType& dataType) {
    switch (dataType) {
        case DataType::DT_VARIANT:
            return LAROD_TENSOR_DATA_TYPE_UNSPECIFIED;
        case DataType::DT_BOOL:
            return LAROD_TENSOR_DATA_TYPE_BOOL;
        case DataType::DT_UINT8:
            return LAROD_TENSOR_DATA_TYPE_UINT8;
        case DataType::DT_INT8:
            return LAROD_TENSOR_DATA_TYPE_INT8;
        case DataType::DT_UINT16:
            return LAROD_TENSOR_DATA_TYPE_UINT16;
        case DataType::DT_INT16:
            return LAROD_TENSOR_DATA_TYPE_INT16;
        case DataType::DT_UINT32:
            return LAROD_TENSOR_DATA_TYPE_UINT32;
        case DataType::DT_INT32:
            return LAROD_TENSOR_DATA_TYPE_INT32;
        case DataType::DT_UINT64:
            return LAROD_TENSOR_DATA_TYPE_UINT64;
        case DataType::DT_INT64:
            return LAROD_TENSOR_DATA_TYPE_INT64;
        /*
        case DataType::DT_:
          return LAROD_TENSOR_DATA_TYPE_FLOAT16;
        */
        case DataType::DT_FLOAT:
            return LAROD_TENSOR_DATA_TYPE_FLOAT32;
        /*
        case DataType::DT_:
          return LAROD_TENSOR_DATA_TYPE_FLOAT64;
        */
        default:
        case DataType::DT_INVALID:
            return LAROD_TENSOR_DATA_TYPE_INVALID;
    }
}

// Convert from larod to TensorFlow datatype
inline const DataType LarodToTfDataType(const larodTensorDataType& dataType) {
    switch (dataType) {
        case LAROD_TENSOR_DATA_TYPE_UNSPECIFIED:
            return DataType::DT_VARIANT;
        case LAROD_TENSOR_DATA_TYPE_BOOL:
            return DataType::DT_BOOL;
        case LAROD_TENSOR_DATA_TYPE_UINT8:
            return DataType::DT_UINT8;
        case LAROD_TENSOR_DATA_TYPE_INT8:
            return DataType::DT_INT8;
        case LAROD_TENSOR_DATA_TYPE_UINT16:
            return DataType::DT_UINT16;
        case LAROD_TENSOR_DATA_TYPE_INT16:
            return DataType::DT_INT16;
        case LAROD_TENSOR_DATA_TYPE_UINT32:
            return DataType::DT_UINT32;
        case LAROD_TENSOR_DATA_TYPE_INT32:
            return DataType::DT_INT32;
        case LAROD_TENSOR_DATA_TYPE_UINT64:
            return DataType::DT_UINT64;
        case LAROD_TENSOR_DATA_TYPE_INT64:
            return DataType::DT_INT64;
        /*
        case LAROD_TENSOR_DATA_TYPE_FLOAT16:
          return DataType::DT_;
        */
        case LAROD_TENSOR_DATA_TYPE_FLOAT32:
            return DataType::DT_FLOAT;
        /*
        case LAROD_TENSOR_DATA_TYPE_FLOAT64:
          return DataType::DT_;
        */
        case LAROD_TENSOR_DATA_TYPE_INVALID:
        default:
            return DataType::DT_INVALID;
    }
}

// Size of larod datatype
inline const uint8_t LarodDataTypeSize(const larodTensorDataType& dataType) {
    switch (dataType) {
        case LAROD_TENSOR_DATA_TYPE_BOOL:
        case LAROD_TENSOR_DATA_TYPE_INT8:
        case LAROD_TENSOR_DATA_TYPE_UINT8:
        case LAROD_TENSOR_DATA_TYPE_UNSPECIFIED:
            return 1;
        case LAROD_TENSOR_DATA_TYPE_FLOAT16:
        case LAROD_TENSOR_DATA_TYPE_INT16:
        case LAROD_TENSOR_DATA_TYPE_UINT16:
            return 2;
        case LAROD_TENSOR_DATA_TYPE_FLOAT32:
        case LAROD_TENSOR_DATA_TYPE_INT32:
        case LAROD_TENSOR_DATA_TYPE_UINT32:
            return 4;
        case LAROD_TENSOR_DATA_TYPE_FLOAT64:
        case LAROD_TENSOR_DATA_TYPE_INT64:
        case LAROD_TENSOR_DATA_TYPE_UINT64:
            return 8;
        case LAROD_TENSOR_DATA_TYPE_INVALID:
        default:
            return 0;
    }
}

// Print formatted error message
void Inference::PrintError(const char* msg, larodError* error) {
    stringstream ss;
    ss << msg;
    if (nullptr != error) {
        ss << " (" << error->msg << ")";
    }
    ERRORLOG << ss.str().c_str() << endl;
}

// Print formatted error message with error number
void Inference::PrintErrorWithErrno(const char* msg) {
    stringstream ss;
    ss << msg << " (" << strerror(errno) << ")";
    ERRORLOG << ss.str().c_str() << endl;
}

// Print tensor
void Inference::PrintTensorProtoDebug(const TensorProto& tp) {
    cout << "dtype: " << DataType_Name(tp.dtype()) << endl;
    cout << "tensor_shape: " << tp.tensor_shape().DebugString() << endl;
    cout << "version_number: " << tp.version_number() << endl;
    cout << "tensor_content size: " << tp.tensor_content().length() << endl;
    if (0 < tp.half_val_size()) {
        cout << tp.half_val_size() << " half_vals:" << endl << " ";
        for (auto val : tp.half_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
    if (0 < tp.float_val_size()) {
        cout << tp.float_val_size() << " float_vals:" << endl << " ";
        for (auto val : tp.float_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
    if (0 < tp.double_val_size()) {
        cout << tp.double_val_size() << " double_vals:" << endl << " ";
        for (auto val : tp.double_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
    if (0 < tp.int_val_size()) {
        cout << tp.int_val_size() << " int_vals:" << endl << " ";
        for (auto val : tp.int_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
    if (0 < tp.string_val_size()) {
        cout << tp.string_val_size() << " string_vals:" << endl << " ";
        for (auto val : tp.string_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
    if (0 < tp.scomplex_val_size()) {
        cout << tp.scomplex_val_size() << " scomplex_vals:" << endl << " ";
        for (auto val : tp.scomplex_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
    if (0 < tp.int64_val_size()) {
        cout << tp.int64_val_size() << " int64_vals:" << endl << " ";
        for (auto val : tp.int64_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
    if (0 < tp.bool_val_size()) {
        cout << tp.bool_val_size() << " bool_vals:" << endl << " ";
        for (auto val : tp.bool_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
    if (0 < tp.dcomplex_val_size()) {
        cout << tp.dcomplex_val_size() << " dcomplex_vals:" << endl << " ";
        for (auto val : tp.dcomplex_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
    if (0 < tp.resource_handle_val_size()) {
        cout << tp.float_val_size() << " resource_handle_vals" << endl;
    }
    if (0 < tp.variant_val_size()) {
        cout << tp.float_val_size() << " resource_handle_vals" << endl;
    }
    if (0 < tp.uint32_val_size()) {
        cout << tp.uint32_val_size() << " uint32_vals:" << endl << " ";
        for (auto val : tp.uint32_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
    if (0 < tp.uint64_val_size()) {
        cout << tp.uint64_val_size() << " uint64_vals:" << endl << " ";
        for (auto val : tp.uint64_val()) {
            cout << " " << val;
        }
        cout << endl;
    }
}

void Inference::PrintTensorInfo(larodTensor** tensors, size_t numTensors) {
    larodError* error = NULL;
    char message[255];
    for (size_t i = 0; i < numTensors; i++) {
        larodTensor* tensor = tensors[i];
        const larodTensorDims* dims = larodGetTensorDims(tensor, &error);
        if (!dims) {
            snprintf(message,
                     sizeof(message),
                     "Could not get dimensions of tensor (%d): %s",
                     error->code,
                     error->msg);
            ERRORLOG << message << endl;
            goto end;
        }

        const larodTensorPitches* pitches = larodGetTensorPitches(tensor, &error);
        if (!pitches) {
            snprintf(message,
                     sizeof(message),
                     "Could not get pitches of tensor (%d): %s",
                     error->code,
                     error->msg);
            ERRORLOG << message << endl;
            goto end;
        }

        larodTensorDataType dataType = larodGetTensorDataType(tensor, &error);
        if (dataType == LAROD_TENSOR_DATA_TYPE_INVALID) {
            snprintf(message,
                     sizeof(message),
                     "Could not get data type of tensor (%d): %s",
                     error->code,
                     error->msg);
            ERRORLOG << message << endl;
            goto end;
        }

        larodTensorLayout layout = larodGetTensorLayout(tensor, &error);
        if (layout == LAROD_TENSOR_LAYOUT_INVALID) {
            snprintf(message,
                     sizeof(message),
                     "Could not get layout of tensor (%d): %s",
                     error->code,
                     error->msg);
            ERRORLOG << message << endl;
            goto end;
        }

        const char* tensorName = larodGetTensorName(tensor, &error);
        if (!tensorName) {
            snprintf(message,
                     sizeof(message),
                     "Could not get name of tensor (%d): %s",
                     error->code,
                     error->msg);
            ERRORLOG << message << endl;
            goto end;
        }

        snprintf(message,
                 sizeof(message),
                 "  name = \"%s\", dataType = %s, layout = %s, dims.len = %zu",
                 tensorName,
                 DATA_TYPES[dataType],
                 LAYOUTS[layout],
                 dims->len);
        TRACELOG << message << endl;
        for (size_t j = 0; j < dims->len; j++) {
            snprintf(message,
                     sizeof(message),
                     "    Dim %zu: size = %zu element(s), pitch = %zu byte(s)",
                     j,
                     dims->dims[j],
                     pitches->pitches[j]);
            TRACELOG << message << endl;
        }
    }

end:
    larodClearError(&error);
}

// Create temporary tile
bool Inference::CreateTmpFile(FILE*& file, int& fd, const void* data, const size_t data_size) {
    // memfd_create does not work inside containers
#ifdef USE_MEMFD_CREATE
    (void)file;
    // Create anonymous file
    fd = memfd_create(tmpnam(nullptr), MFD_ALLOW_SEALING);
    if (0 > fd) {
        PrintErrorWithErrno("Failed to create temporary file");
        return false;
    }
#else
    // Create temporary file
    file = tmpfile();
    if (nullptr == file) {
        PrintErrorWithErrno("Failed to create temporary file");
        return false;
    }

    // Create file descriptor
    fd = fileno(file);
    if (0 > fd) {
        PrintErrorWithErrno("Failed to get file descriptor for temporary file");
        fclose(file);
        return false;
    }
#endif

    // Copy data if available
    if (0 < data_size && nullptr != data) {
#ifdef USE_MEMFD_CREATE
        auto written = write(fd, data, data_size);
#else
        auto written = fwrite(data, 1, data_size, file);
        fflush(file);
#endif
    }

    return true;
}

// Close file
void Inference::CloseTmpFile(FILE*& file, const int& fd) {
    if (file != nullptr) {
        fclose(file);
    }

    if (fd >= 0) {
        close(fd);
    }
}

// Close files
void Inference::CloseTmpFiles(vector<pair<FILE*, int>>& tmpFiles) {
    for (auto& tmp : tmpFiles) {
        CloseTmpFile(tmp.first, tmp.second);
    }
    tmpFiles.clear();
}

bool Inference::LoadModel(larodConnection& conn,
                          const char* modelFile,
                          const larodChip chip,
                          const larodAccess access) {
    string modelName;
    larodModel* loadedModel;
    larodError* error = nullptr;
    auto fpModel = fopen(modelFile, "rb");
    if (nullptr == fpModel) {
        stringstream ss;
        ss << "Failed to open model file: " << modelFile;
        PrintErrorWithErrno(ss.str().c_str());

        return false;
    }

    const auto fd = fileno(fpModel);
    if (0 > fd) {
        stringstream ss;
        ss << "Failed to get file descriptor for " << modelFile;
        PrintErrorWithErrno(ss.str().c_str());
        fclose(fpModel);
        return false;
    }

    if (modelName.empty()) {
        modelName.assign(basename(modelFile));
    }

    loadedModel = larodLoadModel(&conn, fd, chip, access, modelName.c_str(), nullptr, &error);
    if (nullptr == loadedModel) {
        stringstream ss;
        ss << "Failed to load model " << modelName.c_str();
        PrintError(ss.str().c_str(), error);
        larodClearError(&error);
        fclose(fpModel);
        return false;
    }

    fclose(fpModel);
    _models.insert(make_pair(modelFile, loadedModel));
    return true;
}

bool Inference::SetupPreprocessing(tensorflow::TensorProto tp,
                                   larodTensor* tensor,
                                   vector<pair<FILE*, int>>& inFiles,
                                   u_int32_t stream,
                                   uint32_t& frame_ref,
                                   larodError*& error) {
    void* larodInputAddr = MAP_FAILED;

    // Get request dimensions
    size_t requestSize = 1;
    larodTensorDims dims;
    dims.len = tp.tensor_shape().dim_size();
    for (auto j = 0; j < dims.len; j++) {
        size_t dim = tp.tensor_shape().dim(j).size();
        dims.dims[j] = dim;
        requestSize *= dim;
    }
    TRACELOG << "Request size: " << requestSize << endl;

    // Get model dimension
    const larodTensorDims* modelDims = larodGetTensorDims(tensor, &error);
    if (nullptr == modelDims) {
        PrintError("Failed to get tensor data dimensions", error);
        return false;
    }
    size_t modelSize = 1;
    for (auto j = 0; j < modelDims->len; j++) {
        modelSize *= modelDims->dims[j];
    }
    TRACELOG << "Model size: " << modelSize << endl;

    // LAROD_TENSOR_LAYOUT_NHWC format assumed
    int requestHeight = dims.dims[1];
    int requestWidth = dims.dims[2];
    int modelHeight = modelDims->dims[1];
    int modelWidth = modelDims->dims[2];
    ;
    TRACELOG << "Request image size " << requestWidth << "x" << requestHeight << endl;
    TRACELOG << "Model image size " << modelWidth << "x" << modelHeight << endl;

    bool isMemoryMappedFile = tp.dtype() == tensorflow::DataType::DT_STRING;
    bool isRequestForImageFromStream = stream != 0;

    // Convert request image to file descriptor
    FILE* tmpFile = nullptr;
    int tmpFd = -1;
    if (isMemoryMappedFile) {
        string filename = tp.string_val(0);
        TRACELOG << "Input file: " << filename << endl;
        tmpFd = shm_open(filename.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
        if (tmpFd < 0) {
            ERRORLOG << "Can not open shared memory file " << filename << endl;
            return false;
        }
    } else if (isRequestForImageFromStream) {
        TRACELOG << "Got request to use image from stream " << stream << endl;

        size_t size;
        void* data;
        if (!_captureService->GetImgDataFromStream(stream, &data, size, frame_ref)) {
            ERRORLOG << "Could not get data from stream" << endl;
            return false;
        }

        TRACELOG << "Got data of size " << size << endl;

        // TODO: Try to set tmpFd directly to the fd from the vdo_buffer
        if (!CreateTmpFile(tmpFile, tmpFd, data, size)) {
            TRACELOG << "Failed creating tmp file" << size << endl;
            return false;
        }
    } else {
        TRACELOG << "Input ByteSize: " << tp.tensor_content().size() << endl;
        if (!CreateTmpFile(tmpFile,
                           tmpFd,
                           tp.tensor_content().data(),
                           tp.tensor_content().size())) {
            return false;
        }
    }
    inFiles.push_back(make_pair(tmpFile, tmpFd));

    // Check if resize is needed
    if (requestSize == modelSize && requestWidth == modelWidth && requestHeight == modelHeight) {
        if (!larodSetTensorFd(tensor, tmpFd, &error)) {
            PrintError("Failed to set input tensor file descriptor", error);
            return false;
        }
        return true;
    }

    // Create preprocessing maps
    _ppMap = larodCreateMap(&error);
    if (!_ppMap) {
        PrintError("Could not create preprocessing larodMap", error);
        return false;
    }

    // We only support YUV here for now. In the future we should perhaps allow for
    // other formats in the stream.
    const char* inputFormat = isRequestForImageFromStream ? "nv12" : "rgb-interleaved";

    if (!larodMapSetStr(_ppMap, "image.input.format", inputFormat, &error)) {
        PrintError("Failed setting preprocessing parameters", error);
        return false;
    }

    if (!larodMapSetIntArr2(_ppMap, "image.input.size", requestWidth, requestHeight, &error)) {
        PrintError("Failed setting preprocessing parameters", error);
        return false;
    }
    if (!larodMapSetStr(_ppMap, "image.output.format", "rgb-interleaved", &error)) {
        PrintError("Failed setting preprocessing parameters", error);
        return false;
    }
    if (!larodMapSetIntArr2(_ppMap, "image.output.size", modelWidth, modelHeight, &error)) {
        PrintError("Failed setting preprocessing parameters", error);
        return false;
    }

    _ppModel =
        larodLoadModel(_conn, -1, LAROD_CHIP_LIBYUV, LAROD_ACCESS_PRIVATE, "", _ppMap, &error);
    if (!_ppModel) {
        PrintError("Unable to load preprocessing model", error);
        return false;
    }

    // Create preprocessing tensors
    _ppInputTensors = larodCreateModelInputs(_ppModel, &_ppNumInputs, &error);
    if (!_ppInputTensors) {
        PrintError("Failed retrieving preprocessing input tensors", error);
        return false;
    }
    _ppOutputTensors = larodCreateModelOutputs(_ppModel, &_ppNumOutputs, &error);
    if (!_ppOutputTensors) {
        PrintError("Failed retrieving output tensors", error);
        return false;
    }

    // Create preprocessing intermediate buffer
    FILE* larodInputFile = nullptr;
    int larodInputFd = -1;
    if (!CreateTmpFile(larodInputFile, larodInputFd, nullptr, 0)) {
        return false;
    }
    inFiles.push_back(make_pair(larodInputFile, larodInputFd));

    // Set preprocessing buffers
    if (!larodSetTensorFd(_ppInputTensors[0], tmpFd, &error)) {
        PrintError("Failed to set preprocessing input tensor file descriptor", error);
        return false;
    }
    if (!larodSetTensorFd(_ppOutputTensors[0], larodInputFd, &error)) {
        PrintError("Failed to set preprocessing output tensor file descriptor", error);
        return false;
    }
    if (!larodSetTensorFd(tensor, larodInputFd, &error)) {
        PrintError("Failed to set input tensor file descriptor", error);
        return false;
    }

    return true;
}

// Create input tensors
// NB! No cleanup is performed here upon failure. The calling function is
//     expected to handle that.
bool Inference::SetupInputTensors(larodModel*& model,
                                  const google::protobuf::Map<string, TensorProto>& inputs,
                                  vector<pair<FILE*, int>>& inFiles,
                                  const u_int32_t stream,
                                  uint32_t& frame_ref,
                                  larodError*& error) {
    // Setup input tensors
    _inputTensors = larodCreateModelInputs(model, &_numInputs, &error);
    if (nullptr == _inputTensors) {
        PrintError("Failed retrieving input tensors", error);
        return false;
    }

    TRACELOG << "Model NumInputs: " << _numInputs << endl;
    if (inputs.size() != _numInputs) {
        ERRORLOG << "Predict request has " << inputs.size() << " inputs but model has "
                 << _numInputs << endl;
        return false;
    }

    int i = 0;
    for (auto& [input_name, tpa] : inputs) {
        tensorflow::TensorProto tp = tpa;
        TRACELOG << "Input name: " << input_name << endl;
        if (!SetupPreprocessing(tp, _inputTensors[i], inFiles, stream, frame_ref, error)) {
            return false;
        }

        //    const larodTensorLayout setLayout = LAROD_TENSOR_LAYOUT_NHWC;
        //    if (!larodSetTensorLayout(inputTensors[i], setLayout, &error)) {
        //      PrintError("Failed to set tensor layout", error);
        //      return false;
        //    }

        i++;
    }
    return true;
}

// Create output tensors
// NB! No cleanup is performed here upon failure. The calling function is
//     expected to handle that.
bool Inference::SetupOutputTensors(larodModel*& model,
                                   vector<pair<FILE*, int>>& outFiles,
                                   larodError*& error) {
    FILE* tmpFile = nullptr;
    int tmpFd = -1;

    // Setup output tensors
    _outputTensors = larodCreateModelOutputs(model, &_numOutputs, &error);
    if (nullptr == _outputTensors) {
        PrintError("Failed retrieving input tensors", error);
        return false;
    }
    if (_numOutputs < 1) {
        ERRORLOG << "Tensor has less than 1 input" << endl;
        return false;
    }

    // Create temporary files
    for (auto i = 0; i < _numOutputs; i++) {
        if (!CreateTmpFile(tmpFile, tmpFd, nullptr, 0)) {
            return false;
        }
        outFiles.push_back(make_pair(tmpFile, tmpFd));
        if (!larodSetTensorFd(_outputTensors[i], tmpFd, &error)) {
            PrintError("Failed to set output tensor file descriptor", error);
            return false;
        }
    }

    return true;
}

// Convert larod response to gRPC message
// NB! No cleanup is performed here upon failure. The calling function is
//     expected to handle that.
bool Inference::LarodOutputToPredictResponse(PredictResponse*& response,
                                             const ModelSpec& model_spec,
                                             larodModel*& model,
                                             vector<pair<FILE*, int>>& outFiles,
                                             larodError*& error) {
    for (auto i = 0; i < _numOutputs; i++) {
        larodTensor* tensor = _outputTensors[i];
        TensorProto output;
        string* tensor_content = output.mutable_tensor_content();
        auto dataType = larodGetTensorDataType(tensor, &error);
        if (LAROD_TENSOR_DATA_TYPE_INVALID == dataType) {
            PrintError("Failed to get tensor data type", error);
            return false;
        }
        output.set_dtype(LarodToTfDataType(dataType));
        auto larodTensorDims = larodGetTensorDims(tensor, &error);
        if (nullptr == larodTensorDims) {
            PrintError("Could not get output tensor dimension", error);
            return false;
        }
        size_t outSize = LarodDataTypeSize(dataType);
        for (auto j = 0; j < larodTensorDims->len; j++) {
            outSize *= larodTensorDims->dims[j];
            auto dim = output.mutable_tensor_shape()->add_dim();
            dim->set_size(larodTensorDims->dims[j]);
            dim->set_name("size");
        }
        tensor_content->resize(outSize);
        int fd = outFiles[i].second;
        if (0 > pread(fd, const_cast<char*>(tensor_content->data()), outSize, 0)) {
            PrintErrorWithErrno("Failed to read data from output file descriptor");
            return false;
        }
        const char* tensorName = larodGetTensorName(tensor, &error);
        if (nullptr == tensorName) {
            PrintError("Could not get name of tensor", error);
            return false;
        }

        output.set_version_number(0);
        TRACELOG << "Tensor " << tensorName << " size = " << outSize << endl;
        (*response->mutable_outputs())[tensorName] = output;
    }

    response->mutable_model_spec()->CopyFrom(model_spec);
    return true;
}
}  // namespace acap_runtime
