/*************************************************************** -*- C++ -*- ***
 * Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 *******************************************************************************/

#include "common/Logger.h"
#include "common/RestClient.h"
#include "common/ServerHelper.h"
#include "cudaq/utils/cudaq_utils.h"
#include <fstream>
#include <thread>
#include <iostream>
namespace cudaq {

/// @brief The OQCServerHelper class extends the ServerHelper class to handle
/// interactions with the OQC server for submitting and retrieving quantum
/// computation jobs.
class OQCServerHelper : public ServerHelper {
private:

  /// @brief RestClient used for HTTP requests.

  /// @brief Helper method to retrieve the value of an environment variable.
  std::string getEnvVar(const std::string &key) const;

  /// @brief Helper method to check if a key exists in the configuration.
  bool keyExists(const std::string &key) const;

  std::vector<std::string> getJobID(int n);

  std::string makeConfig(int shots);


public:
  RestClient client;


  /// @brief Returns the name of the server helper.
  const std::string name() const override { return "OQC"; }

  /// @brief Returns the headers for the server requests.
  RestHeaders getHeaders() override;

  /// @brief Initializes the server helper with the provided backend
  /// configuration.
  void initialize(BackendConfig config) override;

  /// @brief Creates a quantum computation job using the provided kernel
  /// executions and returns the corresponding payload.
  ServerJobPayload createJob(std::vector<KernelExecution> &circuitCodes) override;

  /// @brief Extracts the job ID from the server's response to a job submission.
  std::string extractJobId(ServerMessage &postResponse) override;

  /// @brief Constructs the URL for retrieving a job based on the server's
  /// response to a job submission.
  std::string constructGetJobPath(ServerMessage &postResponse) override;

  /// @brief Constructs the URL for retrieving a job based on a job ID.
  std::string constructGetJobPath(std::string &jobId) override;

  /// @brief Constructs the URL for retrieving the results of a job based on the
  /// server's response to a job submission.
  std::string constructGetResultsPath(ServerMessage &postResponse);

  /// @brief Constructs the URL for retrieving the results of a job based on a
  /// job ID.
  std::string constructGetResultsPath(std::string &jobId);

  /// @brief Retrieves the results of a job using the provided path.
  ServerMessage getResults(std::string &resultsGetPath);

  /// @brief Checks if a job is done based on the server's response to a job
  /// retrieval request.
  bool jobIsDone(ServerMessage &getJobResponse) override;

  /// @brief Processes the server's response to a job retrieval request and
  /// maps the results back to sample results.
  cudaq::sample_result processResults(ServerMessage &postJobResponse) override;
};

// Initialize the OQC server helper with a given backend configuration
void OQCServerHelper::initialize(BackendConfig config) {
  cudaq::info("Initializing OQC Backend.");
  std::cout<<"initing backemd\n";
  // Move the passed config into the member variable backendConfig
  backendConfig = std::move(config);
  // Set the necessary configuration variables for the OQC API
  backendConfig["url"] = "http://localhost:5000";
  // config.find("url") != config.end()
  //                            ? config["url"]
  //                            : "http://localhost:5000";
  backendConfig["version"] = "v0.3";
  backendConfig["user_agent"] = "cudaq/0.3.0";
  backendConfig["target"] = config.find("qpu") != config.end() ? config["qpu"] : "simulator";
  backendConfig["qubits"] = 8;
  // Retrieve the API key from the environment variables
  backendConfig["email"] = "software+no-reply@oxfordquantumcircuits.com"; //getEnvVar("OQC_EMAIL");
  backendConfig["password"] = std::string("softwAre123!"); //getEnvVar("OQC_PASSWORD");
  // Construct the API job path
  backendConfig["job_path"] = "/tasks";// backendConfig["url"] + "/tasks";
}

// Retrieve an environment variable
std::string OQCServerHelper::getEnvVar(const std::string &key) const {
  // Get the environment variable
  const char *env_var = std::getenv(key.c_str());
  // If the variable is not set, throw an exception
  if (env_var == nullptr) {
    throw std::runtime_error(key + " environment variable is not set.");
  }
  // Return the variable as a string
  return std::string(env_var);
}

// Check if a key exists in the backend configuration
bool OQCServerHelper::keyExists(const std::string &key) const {
  return backendConfig.find(key) != backendConfig.end();
}

std::vector<std::string> OQCServerHelper::getJobID(int n){
    // RestHeaders headers = this -> getHeaders();
    RestHeaders headers = OQCServerHelper::getHeaders();
    std::cout<<"getting task id\n";
    nlohmann::json j;
    std::vector<std::string> output;
    // nlohmann::json_v3_11_1::json response = client.get(backendConfig.at("url"), backendConfig.at("job_path")+"?n=" + std::to_string(n), headers);
    for(int i = 0; i < n; ++i){
      nlohmann::json_v3_11_1::json response = client.post(backendConfig.at("url"), backendConfig.at("job_path"), j, headers);
      output.push_back(response[0]);
    }
    // std::cout << response << "\n";
    return output;
}

std::string OQCServerHelper::makeConfig(int shots){
    return "{\"$type\": \"<class 'scc.compiler.config.CompilerConfig'>\", \"$data\": {\"repeats\": "+std::to_string(shots)+", \"repetition_period\": null, \"results_format\": {\"$type\": \"<class 'scc.compiler.config.QuantumResultsFormat'>\", \"$data\": {\"format\": {\"$type\": \"<enum 'scc.compiler.config.InlineResultsProcessing'>\", \"$value\": 1}, \"transforms\": {\"$type\": \"<enum 'scc.compiler.config.ResultsFormatting'>\", \"$value\": 3}}}, \"metrics\": {\"$type\": \"<enum 'scc.compiler.config.MetricsType'>\", \"$value\": 6}, \"active_calibrations\": [], \"optimizations\": {\"$type\": \"<class 'scc.compiler.config.Tket'>\", \"$data\": {\"tket_optimizations\": {\"$type\": \"<enum 'scc.compiler.config.TketOptimizations'>\", \"$value\": 30}}}}}";
}

// Create a job for the OQC quantum computer
ServerJobPayload OQCServerHelper::createJob(std::vector<KernelExecution> &circuitCodes) {
  // Check if the necessary keys exist in the configuration
  std::cout<<"creating jobn inside server helper\n";
  if (!keyExists("target") || !keyExists("qubits") || !keyExists("job_path"))
    throw std::runtime_error("Key doesn't exist in backendConfig.");
  std::vector<ServerMessage> jobs;
  int number_of_circuits = static_cast<int>(circuitCodes.size());
  std::cout<<"number of circuits " << std::to_string(number_of_circuits) << "\n";
  std::vector<std::string> task_ids = OQCServerHelper::getJobID(number_of_circuits);

  std::cout<<"We have the task ids now\n";

  

  for (int i = 0; i<number_of_circuits; i++){
    nlohmann::json j;
    j["tasks"] = std::vector<nlohmann::json>();
    // Construct the job message
    std::cout << "creating job number\n";
    nlohmann::json job;
    job["task_id"] = task_ids[i];
    job["config"] = makeConfig(static_cast<int>(shots));
    job["program"] = circuitCodes[i].code;
    j["tasks"].push_back(job);
    std::cout<<j<<"\n";
    jobs.push_back(j);
  }

  // Return a tuple containing the job path, headers, and the job message
  return std::make_tuple(backendConfig.at("job_path")+"/submit", getHeaders(), jobs);
}

// From a server message, extract the job ID
std::string OQCServerHelper::extractJobId(ServerMessage &postResponse) {
  // If the response does not contain the key 'id', throw an exception
  if (!postResponse.contains("task_id"))
    throw std::runtime_error("ServerMessage doesn't contain 'id' key.");

  // Return the job ID from the response
  return postResponse.at("task_id");
}

// Construct the path to get a job
std::string OQCServerHelper::constructGetJobPath(ServerMessage &postResponse) {
  // Check if the necessary keys exist in the response and the configuration
  if (!postResponse.contains("task_id"))
    throw std::runtime_error(
        "ServerMessage doesn't contain 'results_url' key.");

  // Return the job path
  return backendConfig.at("job_path") + postResponse.at("task_id").get<std::string>() +"/results";
}

// Overloaded version of constructGetJobPath for jobId input
std::string OQCServerHelper::constructGetJobPath(std::string &jobId) {
  if (!keyExists("job_path"))
    throw std::runtime_error("Key 'job_path' doesn't exist in backendConfig.");

  // Return the job path
  return backendConfig.at("job_path") + jobId + "/results";
}

// Construct the path to get the results of a job
std::string
OQCServerHelper::constructGetResultsPath(ServerMessage &postResponse) {
  // Return the results path
  return backendConfig.at("job_path") + postResponse.at("task_id").get<std::string>() +"/results";
}

// Overloaded version of constructGetResultsPath for jobId input
std::string OQCServerHelper::constructGetResultsPath(std::string &jobId) {
  if (!keyExists("job_path"))
    throw std::runtime_error("Key 'job_path' doesn't exist in backendConfig.");

  // Return the results path
  return backendConfig.at("job_path") + jobId + "/results";
}

// Get the results from a given path
ServerMessage OQCServerHelper::getResults(std::string &resultsGetPath) {
  RestHeaders headers = getHeaders();
  // Return the results from the client
  return client.get(resultsGetPath, "", headers);
}

// Check if a job is done
bool OQCServerHelper::jobIsDone(ServerMessage &getJobResponse) {
  // Check if the necessary keys exist in the response
  if (!getJobResponse.contains("jobs"))
    throw std::runtime_error("ServerMessage doesn't contain 'jobs' key.");

  auto &jobs = getJobResponse.at("jobs");

  if (jobs.empty() || !jobs[0].contains("status"))
    throw std::runtime_error(
        "ServerMessage doesn't contain 'status' key in the first job.");

  // Return whether the job is completed
  return jobs[0].at("status").get<std::string>() == "COMPLETED";
}

// Process the results from a job
cudaq::sample_result
OQCServerHelper::processResults(ServerMessage &postJobResponse) {
  // Construct the path to get the results
  auto resultsGetPath = constructGetResultsPath(postJobResponse);
  // Get the results
  cudaq::ServerMessage results = getResults(resultsGetPath);
  cudaq::CountsDictionary counts = results.at("results");
  // Create an execution result
  cudaq::ExecutionResult executionResult(counts);
  // Return a sample result
  return cudaq::sample_result(executionResult);
}

// Get the headers for the API requests
RestHeaders OQCServerHelper::getHeaders() {
  // Check if the necessary keys exist in the configuration
  if (!keyExists("email") || !keyExists("password"))
    throw std::runtime_error("Key doesn't exist in backendConfig.");

  // Construct the headers
  RestHeaders headers;
  // nlohmann::json_v3_11_1::json json_for_headers("{\'email\': \'"+ backendConfig.at("email") + "\', \'password\': \'" + backendConfig.at("password") + "\'}");
  // std::string json_for_headers ="{\'email\': \'"+ backendConfig.at("email") + "\', \'password\': \'" + backendConfig.at("password") + "\'}";
  // std::map<std::string, std::string> headers;
  // std::map<std::string, std::string> auth_map = { {"email", backendConfig.at("email")}, {"password", backendConfig.at("password")}};

  // nlohmann::json tmpjson_for_headers(auth_map);
  // std::cout<< tmpjson_for_headers << " " << typeid(tmpjson_for_headers).name() <<"\n";
  nlohmann::json j;
  j["email"] = backendConfig.at("email");

  // std::string pw = R"softwAre123!";
  j["password"] =  "softwAre123@"; //backendConfig.at("password");

  std::cout<<"getting headers\n";
  nlohmann::json response = client.post(backendConfig.at("url"), "/auth", j, headers);

  // nlohmann::json response;

  std::string key = response.at("access_token");
  std::cout<< key << "\n";
  std::cout<<"headers got\n";
  headers["Authorization"] = "Bearer "+key;
  headers["Content-Type"] = "application/json";
  // Return the headers
  std::cout<<"returning headers \n";
  return headers;
}

} // namespace cudaq

// Register the OQC server helper in the CUDAQ server helper factory
CUDAQ_REGISTER_TYPE(cudaq::ServerHelper, cudaq::OQCServerHelper, oqc)