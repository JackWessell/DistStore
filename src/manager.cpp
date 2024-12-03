#include <iostream>
#include <memory>
#include <string>
#include <csignal>
#include <signal.h>

#include "gtstore.hpp"
#include "gtnode.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using demo::Manager;
using demo::ClientRequest;
using demo::ManagerReply;

Status ManagerImpl::Handshake(ServerContext *context, const ClientRequest *request, ManagerReply *reply){
  //std::cout << "Received Client Name: " << request->user_id() << std::endl;
  auto nodes_map = state_->GetNodes();
  auto* mutable_map = reply->mutable_nodes();
  mutable_map->insert(nodes_map.begin(), nodes_map.end());
  return grpc::Status::OK;
}
Status ManagerImpl::liveness(ServerContext *context, const NodeAddr *request, Empty *reply){
  std::string key = request->addr();
  TimePoint now = Clock::now();
  heartbeat_->AddNode(key, now);
  //cout << "Node listening at port " << key << " is alive!" << endl;
  return grpc::Status::OK;
}
void GTStoreManager::initialize(const std::string& server_address){
  //initialize our map. For now, this is how we will test our system but will make this a more explicit funcitonality in the future
  server_address_ = server_address;
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(service_.get());
  server_ = builder.BuildAndStart();
  std::cout << "Manager listening on " << server_address << std::endl;
  return;
}
void GTStoreManager::add_node(uint32_t key, const std::string& server_address){
  state_->AddNode(key, server_address);
  return;
}
void GTStoreManager::start_liveness(const std::string& addr){
  TimePoint now = Clock::now();
  heartbeat_->AddNode(addr, now);
  return;
}
void GTStoreManager::wait(){
  server_->Wait();
  return;
}
void GTStoreManager::check_liveness(){
  map<string, TimePoint>::iterator it;
  std::string dropping;
  bool drop = false;
  while(true){
	  for (it = heartbeat_->info_.begin(); it != heartbeat_->info_.end(); it++){
    	  TimePoint then = it->second;
        TimePoint now = Clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - then
        ).count();
        //if a node hasn't responded in x seconds, remove it from the table.
        if(elapsed > 5){
          state_->RemoveValue(it->first);
          dropping = it->first;
          cout << "Node at " << dropping << " is dead." << endl;
          drop = true;
          break;
        }
  	}
    if(drop){
     // cout << "Dropping!!!" << endl;
      state_->RemoveValue(dropping);
      heartbeat_->info_.erase(dropping);
      drop = false;
    }
    sleep(.5);
  }
}
void GTStoreManager::setup(){
  //one worker thread will wait, one will continuously handle the system table and check for liveness.
  std::thread worker1(std::mem_fn(&GTStoreManager::wait), this);
	std::thread worker2(std::mem_fn(&GTStoreManager::check_liveness), this);
  worker1.join();
  worker2.join();
  return;
}

static std::vector<pid_t> process_list;
//signal handling for SIGTERM - this will be used to kill the manager from the test app while automatically killing storage nodes as well.
void handle_sigterm(int signal) {
    for(auto pid : process_list){
        kill(pid, SIGKILL);
    }
    exit(0);
}

// Function to set up our signal handling
void setup_signal_handler() {
    // Create a signal action structure
    struct sigaction sa;
    
    // Initialize it to empty
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    // Register our handler for SIGTERM
    if (sigaction(SIGTERM, &sa, nullptr) == -1) {
        std::cerr << "Failed to set up signal handler\n";
        exit(1);
    }
}

int main(int argc, char **argv){
  int num_nodes;
  int replication;
  //setup the SIGTERM handler for our manager. This will kill all child processes (storage nodes).
  setup_signal_handler();
  std::string master_port;
  if(argc != 7){
    std::cout << "Usage: ./manager -n NUM_STORAGE_NODES -k REPLICATION_COUNT -a SERVER_ADDRESS" << std::endl;
    return 0;
  }
  for(int i = 1; i < argc; i++){
    if(i == 1 && strcmp(argv[i], "-n") != 0){
      std::cout << "Usage: ./manager -n NUM_STORAGE_NODES -k REPLICATION_COUNT -a SERVER_ADDRESS" << std::endl;
      return 0;
    }
    else if(i==1){
      num_nodes = std::stoi(argv[i+1]);
    }
    if(i == 3 && strcmp(argv[i], "-k") != 0){
      std::cout << "Usage: ./manager -n NUM_STORAGE_NODES -k REPLICATION_COUNT -a SERVER_ADDRESS" << std::endl;
      return 0;
    }
    else if(i==3){
      replication = std::stoi(argv[i+1]);
    }
    if(i == 5 && strcmp(argv[i], "-a") != 0){
      std::cout << "Usage: ./manager -n NUM_STORAGE_NODES -k REPLICATION_COUNT -a SERVER_ADDRESS" << std::endl;
      return 0;
    }
    else if(i==5){
      master_port = argv[i+1];
    }
  }
  GTStoreManager manager;
  RandomGenerator gen(42);
  
  manager.initialize(master_port);
  int idx = master_port.find(':');
  int port = stoi(master_port.substr(idx+1));
  cout << port << endl;
  std::string address;
  int child_port = port + 1;
  for(int i = 0; i < num_nodes; i++){
    address = "0.0.0.0:" + to_string(child_port);
    pid_t pid = fork();
    if (pid < 0) {
      std::cerr << "Fork failed: " << strerror(errno) << std::endl;
      return 0;
    }
    //child process
    if (pid == 0){
      //set-up arguments and execute
      std::vector<char*> argv;
      std::string id_str = to_string(i);
      std::string rep_str = to_string(replication);
      std::string n_str = to_string(num_nodes);
      argv.push_back("./build/src/storage");
      argv.push_back("-m");
      argv.push_back(master_port.data());
      argv.push_back("-a");
      argv.push_back(address.data());
      argv.push_back("--id");
      argv.push_back(id_str.data());
      argv.push_back("-k");
      argv.push_back(rep_str.data());
      argv.push_back("-n");
      argv.push_back(n_str.data());
      argv.push_back(nullptr);
      execv(argv[0], argv.data());
      //check for failures
      std::cerr << "Exec failed: " << strerror(errno) << std::endl;
      _exit(EXIT_FAILURE);

    }
    //parent process.
    else{
      //create n duplicate "virtual nodes" to store in the table. This will ensure that upon failure, the keys that the node was responsible for are
      //distributed across multiple nodes.
      for(int j = 0; j < num_nodes; j++){
        process_list.push_back(pid);
        int rand = gen.get_random();
        manager.start_liveness(address);
        manager.add_node(rand, address);
      }
      child_port++;
      continue;
    }
  } 
  //wait on our server for a connection.
  manager.setup();
  return 0;
}