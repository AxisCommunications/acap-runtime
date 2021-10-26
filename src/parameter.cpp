/* Copyright 2021 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include "parameter.h"

namespace acap_runtime {
struct kv_pair {
  const char* key;
  const char* value;
};

static const kv_pair kvs_map[] = {
    {"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"},
    {"key4", "value4"}, {"key5", "value5"},
};

const char* get_value_from_map(const char* key) {
  for (size_t i = 0; i < sizeof(kvs_map) / sizeof(kv_pair); ++i) {
    if (strcmp(key, kvs_map[i].key) == 0) {
      return kvs_map[i].value;
    }
  }
  return "";
}

// Logic and data behind the server's behavior.
Status Parameter::GetValues(ServerContext* context,
    ServerReaderWriter<Response, Request>* stream) {
    Request request;
    while (stream->Read(&request)) {
      Response response;
      response.set_value(get_value_from_map(request.key().c_str()));
      stream->Write(response);
    }
    return Status::OK;
}
}  // namespace acap_runtime