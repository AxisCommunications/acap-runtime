/* Copyright 2020 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <getopt.h>
#include <grpcpp/grpcpp.h>
#include <sstream>
#include <unistd.h>
#include "read_text.h"
#include "inference.h"

#define LOG(level) if (_verbose || #level == "ERROR") std::cerr << #level << " in acap_runtime: "

using namespace std;
using namespace grpc;
using namespace inference_server;

bool _verbose = false;

// Initialize acap-runtime and start gRPC service
void RunServer(
  const string& address,
  const int port,
  const uint64_t chipId,
  const unsigned int time,
  const string& certificateFile,
  const string& keyFile,
  const vector<string>& models)
{
  // Setup gRPC service and credentials
  LOG(INFO) << "RunServer time=" << time << endl;
  stringstream server_address;
  server_address << address << ":" << port;
  Inference service;
  if (service.Init(_verbose, chipId, models)) {
    ServerBuilder builder;
    if (certificateFile.length() == 0 || keyFile.length() == 0) {
      shared_ptr<ServerCredentials> creds = InsecureServerCredentials();
      builder.AddListeningPort(server_address.str(), creds);
    }
    else {
      SslServerCredentialsOptions ssl_opts;
      ssl_opts.pem_root_certs = "";
      string server_cert = read_text(certificateFile.c_str());
      string server_key = read_text(keyFile.c_str());
      SslServerCredentialsOptions::PemKeyCertPair pkcp = {
        server_key.c_str(),
        server_cert.c_str()
      };
      ssl_opts.pem_key_cert_pairs.push_back(pkcp);
      shared_ptr<ServerCredentials> creds = SslServerCredentials(ssl_opts);
      builder.AddListeningPort(server_address.str(), creds);
    }

    // Start gRPC service
    builder.RegisterService(&service);
    unique_ptr<Server> server(builder.BuildAndStart());
    if (!server) {
      LOG(ERROR) << "Could not start gRPC server";
      return;
    }
    cout << "Server listening on " << server_address.str() << endl;

    // Wait for gRPC sercice termination
    if (time > 0) {
      LOG(INFO) << "Server run time (s): " << time << endl;
      sleep(time);
      server->Shutdown();
      LOG(INFO) << "Server shutdown" << endl;
    }
    else
    {
      server->Wait();
    }
  }
}

// Print help
void Usage(const char* name)
{
  cerr << "Usage: " << name
    << " [-v] [-a address ] [-p port] [-j chip-id]  [-t runtime] [-c certificate-file] [-k key-file] [-m model-file] ... [-m model-file]" << endl
    << "  -v    Verbose" << endl
    << "  -a    IP address of server" << endl
    << "  -p    IP port of server" << endl
    << "  -j    Chip id (see larodChip in larod.h)" << endl
    << "  -t    Runtime in seconds (used for test)" << endl
    << "  -c    Certificate file for TLS authentication, insecure channel if omitted" << endl
    << "  -k    Private key file for TLS authentication, insecure channel if omitted" << endl
    << "  -m    Larod model file" << endl;
}

// Main program
// Note: name is _main not to conflict with test framework main()
int AcapRuntime(int argc, char* argv[])
{
  auto port = 9001;
  int opt;
  optind = 0; // Reset opt index
  string address = "0.0.0.0";
  string pem_file = "";
  string key_file = "";
  uint64_t chipId = 0;
  int time = 0; // Run time in seconds, 0 => infinity
  vector<string> models;
  while (-1 != (opt = getopt(argc, argv, "a:hj:m:p:t:c:k:v"))) {
    switch (opt) {
      case 'a':
        address.assign(optarg);
        break;
      case 'h':
        Usage(argv[0]);
        return 0;
      case 'j':
        chipId = atoi(optarg);
        break;
      case 'm':
        models.push_back(optarg);
        break;
      case 'p':
        port = atoi(optarg);
        break;
      case 't':
        time = atoi(optarg);
        break;
      case 'v':
        _verbose = true;
        break;
      case 'c':
        pem_file.assign(optarg);
        break;
      case 'k':
        key_file.assign(optarg);
        break;
      default:
        Usage(argv[0]);
        return 1;
    }
  }

  RunServer(address, port, chipId, time, pem_file, key_file, models);
  return 0;
}
