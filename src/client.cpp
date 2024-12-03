#include <iostream>
#include <memory>
#include <string>


#include "gtstore.hpp"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
//For manager
using demo::Manager;
using demo::ManagerReply;
using demo::ClientRequest;
//For storage nodes
using demo::Storage;
using demo::ClientKey;
using demo::ClientValue;
using demo::Ack;
using demo::KeyValue;
void GTStoreClient::handshake(){
  ClientRequest request;
  request.set_user_id(client_id);
  ManagerReply response;
  ClientContext context;
  Status status = manager_stub->Handshake(&context, request, &response);
  std::map<uint32_t, std::string> new_nodes;
  if (status.ok()) {
      new_nodes = std::map<uint32_t, std::string>(
            response.nodes().begin(), 
            response.nodes().end()
        );
      *storage_nodes = new_nodes;
  } 
  else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return;
  }
}
int GTStoreClient::put(std::string addr, uint32_t key, std::string val){
    std::shared_ptr<Channel> channel;
    channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    std::unique_ptr<Storage::Stub> storage_stub = Storage::NewStub(channel);
    KeyValue request;
    request.set_key(key);
    request.set_value(val);
    //client tells storage node it should replicate - i.e it is the primary node.
    request.set_rep(true);
    Ack response;
    while(true){
      ClientContext context;
      context.set_deadline(
                std::chrono::system_clock::now() + 
                std::chrono::milliseconds(1000) 
            );
      Status status = storage_stub->put(&context, request, &response);
      if(status.ok()){
        std::cout << "Successfully put: \'" << val << "\' onto node: "<< response.id() << " with key: \'" << key << "\'" << std::endl;
        break;
      }
      else{
        cout << "Target storage node has failed. Backing off and re-trying..."  << endl;
        std::this_thread::sleep_for(
                    std::chrono::milliseconds(2000)
                );
        //if a node is down, get new information from manager and try again
        handshake();
        addr = get_node(key);
        channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
        storage_stub = Storage::NewStub(channel);
      }
    }
    return response.id();
}

std::string GTStoreClient::get(std::string addr, uint32_t key){
    std::shared_ptr<Channel> channel;
    channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    std::unique_ptr<Storage::Stub> storage_stub = Storage::NewStub(channel);
    ClientKey request;
    request.set_key(key);
    ClientContext context;
    ClientValue response;
    while(true){
      ClientContext context;
      context.set_deadline(
                std::chrono::system_clock::now() + 
                std::chrono::milliseconds(1000) 
            );
      Status status = storage_stub->get(&context, request, &response);
      if(status.ok()){
        std::cout << "Received \'"<< response.value() << "\' using key \'" << key << "\' from node " << response.id() << std::endl;
        break;
      }
      else{
        cout << "Target storage node has failed. Backing off and re-trying..."  << endl;
        std::this_thread::sleep_for(
                    std::chrono::milliseconds(2000)
                );
        //if a node is down, get new information from manager, update our connection, and try again
        handshake();
        addr = get_node(key);
        channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
        storage_stub = Storage::NewStub(channel);
      }
    }
    return response.value();
}

void GTStoreClient::init(int id){
  //assign our id and initialize contact with the master node
  client_id = id;
  handshake();
  return;
}
void GTStoreClient::get_map(){
  map<uint32_t, string>::iterator it;
  for (it = storage_nodes->begin(); it != storage_nodes->end(); it++){
    std::cout << it->first   << " : " << it->second   << std::endl;
  }
  return;
}
std::string GTStoreClient::get_node(uint32_t key){
  void * hash = malloc(sizeof(uint32_t));
  MurmurHash3_x86_32((void *) &key, sizeof(uint32_t), 0, hash);
  uint32_t val =  ((uint32_t*) hash)[0];
  map<uint32_t, string>::iterator it;
  for(it = storage_nodes->begin(); it != storage_nodes->end(); it++){
    if(val <= it->first){
      return it->second;
    }
  }
  //if at this point, value is larger than all nodes. wrap around and assign to next node.
  return storage_nodes->begin()->second;
}
int main(int argc, char **argv)
{
  std::string target_str = argv[2];
  GTStoreClient greeter(
    grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials())
  );
  greeter.init(10);
  //greeter.get_map();
  //############################# Real Code ################################
  uint32_t val = (uint32_t) stoi(argv[4]);
  std::string addr = greeter.get_node(val);
  if (strcmp(argv[3], "--put") == 0){
    greeter.put(addr, stoi(argv[4]), std::string(argv[6]));
  }
  else if(strcmp(argv[3], "--get") == 0){
    greeter.get(addr, stoi(argv[4]));
  }
  else{
    cout << "Usage: ./client -a ADDRESS --put <KEY> --val <VALUE> OR ./client -a ADDRESS --get <KEY>" << std::endl;
  }
  //############################ Load balancing test ########################
  /*
  std::map<int, int> counter;
  int node;
  for(uint32_t i = 0; i< 100000; i++){
    node = greeter.put(greeter.get_node(i), i, "DEFAULT");
    counter[node]++;
  }
  for(int j = 0; j < 7; j++){
    cout << "Node " << j << " is responsible for " << counter[j] << " keys." << endl;
  }*/
  //############################ Throughput test ############################
  /*
  for(uint32_t i = 0; i < 50; i++){
    greeter.put(greeter.get_node(i), i, "PLACING");
  }
  int num;
  #pragma omp parallel for
  for(uint32_t start = (uint32_t) stoi(argv[1]); start < stoi(argv[1]) + 20000; start++){
    if(start % 2 == 0){
        greeter.put(greeter.get_node(start), start, "DEFAULT");
    }
    else{
        num = std::rand() % 50;
        greeter.get(greeter.get_node(num), num);
    }
  }
  */
  return 0;
}