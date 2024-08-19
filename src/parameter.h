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

#include "keyvaluestore.grpc.pb.h"

#ifdef TEST
#define APP_NAME "acapruntimetest"
#else
#define APP_NAME "acapruntime"
#endif

using namespace grpc;
using namespace keyvaluestore;

namespace acap_runtime {

// Logic and data behind the server's behavior.
class Parameter final : public KeyValueStore::Service {
  public:
    bool Init(const bool verbose);

  private:
    Status GetValues(ServerContext* context, ServerReaderWriter<Response, Request>* stream);

    bool _verbose;
};
}  // namespace acap_runtime
