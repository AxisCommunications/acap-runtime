/* Copyright 2021 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <gtest/gtest.h>
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include "grpcpp/create_channel.h"
#include "keyvaluestore.grpc.pb.h"
#include "memory_use.h"
#include "milli_seconds.h"

int AcapRuntime(int argc, char* argv[]);

using namespace ::testing;
using namespace std;
using namespace grpc;
using namespace google::protobuf;
using namespace keyvaluestore;

const char* target = "localhost:9001";
int logTime = -1;

namespace acap_runtime {
namespace parameter_test {

void LogMemory()
{
  time_t now = time(NULL);
  struct tm *nTime = localtime(&now);
  int nowTime = nTime->tm_hour;
  if (nowTime != logTime)
  {
    int mem = memory_use();
    printf("%d/%d %02d:%02d %d kB\n",
      nTime->tm_mday, nTime->tm_mon, nTime->tm_hour, nTime->tm_min, mem);
    logTime = nowTime;
  }
}

void Service(int seconds)
{
  char timeout[10];
  sprintf(timeout, "%d", seconds);
  const bool verbose = FLAGS_gtest_color == "yes";
  char const * argv[] = {
    "acapruntime", verbose ? "-v" : "",
    "-p", "9001",
    "-t", timeout
     };
  const int argc = sizeof(argv) / sizeof(const char*);
  ASSERT_EQ(0, AcapRuntime(argc, (char**)argv));
}

// Requests each key in the vector and displays the key and its corresponding
// value as a pair
vector<pair<string,string>> GetValues(
  unique_ptr<KeyValueStore::Stub>& stub,
  const vector<string>& keys) {
  vector<pair<string,string>> values;
  ClientContext context;
  auto stream = stub->GetValues(&context);
  for (const auto& key : keys) {
    // Key we are sending to the server.
    Request request;
    request.set_key(key);
    stream->Write(request);

    // Get the value for the sent key
    Response response;
    stream->Read(&response);
    values.push_back(make_pair(key, response.value()));
  }

  stream->WritesDone();
  Status status = stream->Finish();
  EXPECT_TRUE(status.ok());
  return values;
}

TEST(ParameterTest, GetValues)
{
  const bool verbose = FLAGS_gtest_color == "yes";
  vector<string> keys = {
    "key1", "key2", "key3", "key4", "key5", "key1", "key2", "key4"
  };

  thread main(Service, 5);

  shared_ptr<Channel> channel = CreateChannel(target, InsecureChannelCredentials());
  ASSERT_TRUE(channel->WaitForConnected(
    gpr_time_add(gpr_now(GPR_CLOCK_REALTIME),
    gpr_time_from_seconds(5, GPR_TIMESPAN))));
  unique_ptr<KeyValueStore::Stub> stub = KeyValueStore::NewStub(channel);

  vector<pair<string,string>> values = GetValues(stub, keys);
  if (verbose) {
    for (pair<string,string> value: values) {
      cout << value.first << " : " << value.second << endl;
    }
  }

  EXPECT_EQ(8, values.size());
  EXPECT_STREQ("value1", values[0].second.c_str());
  EXPECT_STREQ("value2", values[1].second.c_str());
  EXPECT_STREQ("value3", values[2].second.c_str());
  EXPECT_STREQ("value4", values[3].second.c_str());
  EXPECT_STREQ("value5", values[4].second.c_str());
  EXPECT_STREQ("value1", values[5].second.c_str());
  EXPECT_STREQ("value2", values[6].second.c_str());
  EXPECT_STREQ("value4", values[7].second.c_str());

  main.join();
}

}  // namespace parameter_test
}  // namespace acap_runtime