/* Copyright 2022 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <axsdk/ax_parameter.h>
#include <getopt.h>
#include <grpcpp/grpcpp.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include "read_text.h"
#include "inference.h"
#include "parameter.h"

#define LOG(level) if (_verbose || #level == "ERROR") std::cerr << #level << " in acapruntime: "
#define INSTALLDIR "/usr/local/packages/acapruntime/"

static const char* uds_path = "/tmp/acap-runtime.sock";
static const char* uds_schema = "unix://";
static const char* serverCertificatePath = INSTALLDIR "server.pem";
static const char* serverKeyPath = INSTALLDIR "server.key";

using namespace std;
using namespace grpc;
using namespace acap_runtime;

bool _verbose = false;

// Loop run on the main process
static GMainLoop *loop = NULL;

/**
 * @brief Signals handling
 *
 * @param signal_num Signal number.
 */
static void handle_signals(__attribute__((unused)) int signal_num)
{
  switch (signal_num) {
    case SIGINT:
    case SIGTERM:
    case SIGQUIT:
      g_main_loop_quit(loop);
  }
}

/**
 * @brief Initialize signals
 */
static void init_signals(void)
{
  struct sigaction sa;

  sa.sa_flags = 0;

  sigemptyset(&sa.sa_mask);
  sa.sa_handler = handle_signals;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
}

/**
 * @brief Fetch the value of the parameter as a string
 *
 * @return The value of the parameter as string if successful, NULL otherwise
 */
static char *get_parameter_value(const char *domain, const char *parameter_name)
{
  GError *error = NULL;
  AXParameter *ax_parameter = ax_parameter_new(domain, &error);
  
  if (ax_parameter == NULL) {
    syslog(LOG_ERR, "Error when creating axparameter: %s", error->message);
    g_clear_error(&error);
    return NULL;
  }

  char *parameter_value = NULL;
  if (!ax_parameter_get(ax_parameter, parameter_name, &parameter_value, &error)) {
    free(parameter_value);
    parameter_value = NULL;
  }
  ax_parameter_free(ax_parameter);
  g_clear_error(&error);
  return parameter_value;
}

// Initialize acap-runtime and start gRPC service
int RunServer(
  const string& address,
  const int port,
  const uint64_t chipId,
  const unsigned int time,
  const string& certificateFile,
  const string& keyFile,
  const vector<string>& models)
{
  // Setup gRPC service and credentials
  LOG(INFO) << "RunServer port=" << port <<" chipId=" << chipId << endl;
  ServerBuilder builder;

  // Register channel
  stringstream server_address;
  if (port == 0) {
    server_address << uds_schema << uds_path;
  } else {
    server_address << address << ":" << port;
  }

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
  LOG(INFO) << "Server listening on " << server_address.str() << endl;

  // Register inference service
  Inference inference;
  if (chipId > 0) {
    if (!inference.Init(_verbose, chipId, models)) {
      syslog(LOG_ERR, "Could not Init Inference Service");
      return EXIT_FAILURE;
    }
    builder.RegisterService(&inference);
  }

  // Register parameter service
  Parameter parameter;
  if (!parameter.Init(_verbose)) {
      syslog(LOG_ERR, "Could not Init Parameter Service");
      return EXIT_FAILURE;
  }
  builder.RegisterService(&parameter);

  // Start server
  unique_ptr<Server> server(builder.BuildAndStart());
  if (!server) {
    syslog(LOG_ERR, "Could not start gRPC server");
    return EXIT_FAILURE;
  }

  if (port == 0) {
    // set uds ownership for current user
    if (chown(uds_path, geteuid(), getegid()) != 0)
    {
      syslog(LOG_ERR, "Error setting uds ownership: %s", strerror(errno));
      return EXIT_FAILURE;
    }

    // set uds permission as read-write for current user & group
    if (chmod(uds_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) != 0)
    {
      syslog(LOG_ERR, "Error setting uds permissions: %s", strerror(errno));
      return EXIT_FAILURE;
    }
  }

  // Wait for gRPC sercice termination
  if (time > 0) {
    LOG(INFO) << "Server run time (s): " << time << endl;
    sleep(time);
  }
  else
  {
    /* Run the GLib event loop. */
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
  }

  LOG(INFO) << "Server shutdown" << endl;
  server->Shutdown();
  return EXIT_SUCCESS;
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
  int ipPort = 0;
  string address = "0.0.0.0";
  string pem_file = "";
  string key_file = "";
  uint64_t chipId = 0;
  int time = 0; // Run time in seconds, 0 => infinity

  openlog(NULL, LOG_PID, LOG_USER);
  // Setup signal handling.
  init_signals();

  int opt = 0;
  int parameter_flag = 0;
  optind = 0; // Reset opt index
  vector<string> models;
  LOG(INFO) << "parsing flags " << endl;
  while (-1 != (opt = getopt(argc, argv, "a:hj:m:p:t:c:k:v"))) {
    parameter_flag = 1;
    switch (opt) {
      case 'a':
        address.assign(optarg);
        break;
      case 'h':
        Usage(argv[0]);
        return EXIT_SUCCESS;
      case 'j':
        chipId = atoi(optarg);
        break;
      case 'm':
        models.push_back(optarg);
        break;
      case 'p':
        ipPort = atoi(optarg);
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
        return EXIT_FAILURE;
    }
  }

  // Skipped if command line parameters are provided.
  if(parameter_flag == 0 ){
    LOG(INFO) << "No command line parameters detected, reading parameters from storage" << argv[0] << endl;
    // Read parameters from parameter storage
    char *verbose = get_parameter_value(APP_NAME, "Verbose");
    if (verbose != NULL) {
      _verbose = strcmp(verbose, "yes") == 0;
    }
    char *ip_port = get_parameter_value(APP_NAME, "IpPort");
    if (ip_port != NULL) {   
      ipPort = atoi(ip_port);
    }
    char *chip_id = get_parameter_value(APP_NAME, "ChipId");
    if (chip_id != NULL) { 
      chipId = atoi(chip_id);
    }
    char *useTls = get_parameter_value(APP_NAME, "UseTLS");
    if (useTls != NULL) {
      if (strcmp(useTls, "yes") == 0) {
          pem_file.assign(serverCertificatePath);
          key_file.assign(serverKeyPath);
      }
    }
  }

  LOG(INFO) << "Start " << argv[0] << endl;
  int ret = RunServer(address, ipPort, chipId, time, pem_file, key_file, models);
  LOG(INFO) << "Exit " << argv[0] << endl;
  return ret;
}
