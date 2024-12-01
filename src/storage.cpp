#include "gtstore.hpp"
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::StatusCode;
#define ASSERT(condition, message) \
   do { \
      assert(condition && message); \
   } while (0)


bool StorageImpl::replicate(std::string addr, uint32_t key, std::string val){
	std::shared_ptr<Channel> channel;
    channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    std::unique_ptr<Storage::Stub> storage_stub = Storage::NewStub(channel);
    KeyValue request;
    request.set_key(key);
    request.set_value(val);
	//when sent from a storage node, should not replicate
	request.set_rep(false);
    Ack response;
    ClientContext context;
    Status status = storage_stub->put(&context, request, &response);
    if (status.ok()){
      //std::cout << "Put successful on node "<< response.id() << std::endl;
	  return true;
    }
    else{
      std::cout << status.error_code() << ": " << status.error_message() << std::endl;
      return false;
    }
}

void StorageImpl::put_helper(uint32_t key, std::string val){
	//first, hash the key
	void *hash = malloc(sizeof(uint32_t));
  	MurmurHash3_x86_32((void *) &key, sizeof(uint32_t), 0, hash);
  	uint32_t mapping =  ((uint32_t*) hash)[0];
	
	map<uint32_t, string>::iterator it;
	bool larger = true;
  	for(it = system_state_->nodes.begin(); it != system_state_->nodes.end(); it++){
    	if(mapping <= it->first){
			larger = false;
			break;
    	}
  	}
  	if(larger){
		it = system_state_->nodes.begin();
	}
	
	//now, the iterator should contain the first addr for which the hash of the key is less than or equal to the. This should be our current server.
	ASSERT(it->second.compare(addr_) == 0, "Oh No! put_helper, storage.cpp");
	int i = 0;
	it++;
	//loop until we have added k objects to our backlog.
	//TODO: need to maintain list of nodes we are sending to. Currently, # reps <= k. need #reps = k.
	bool success;
	std::set<std::string> replicated;
	std::set<std::string> failed;
	while(i < k_-1){
		//too many nodes have failed - cannot fully replicate our data.
		if(n_ - failed.size() < k_){
			break;
		}
		if(it == system_state_->nodes.end()){
			it = system_state_->nodes.begin();
		}
		//if the address is our current server, has already been replicated, or has failed to be replicated continue.
		if(it->second.compare(addr_) == 0 || replicated.count(it->second) > 0 || failed.count(it->second) > 0){
			it++;
			continue;
		}
		success = replicate(it->second, key, val);
		if(!success){
			failed.insert(it->second);
			cout << "Replication failed on node " << it->second << ". Adjusting replication protocol..." << endl;
		}
		else{
			replicated.insert(it->second);
			i++;
		}
		it++;
	}
	if(failed.size() > 0){
		callback_->NodeFailure();
	}
	return;
}
Status StorageImpl::put(ServerContext* context, const KeyValue* request, Ack* response){
	uint32_t key = request->key();
	std::string val = request->value();
	bool rep = request->rep();
	storage_state_->AddNode(key, val);
	response->set_id(id_);
	//if we need to replicate, spawn a thread to handle it. This will allow us to continue to handle requests while properly replicated our values.
	if(rep){
		std::thread worker(std::mem_fn(&StorageImpl::put_helper), this, key, val);
		worker.detach();
	}
	return grpc::Status::OK;
}
Status StorageImpl::get(ServerContext* context, const ClientKey* request, ClientValue* response){
	std::string value = storage_state_->Get(request->key());
	if(value.compare("///") == 0){
		return Status(StatusCode::NOT_FOUND, "Key not found in the system");
	}
	response->set_value(value);
	response->set_id(id_);
	return grpc::Status::OK;
}

void GTStoreStorage::wait(){
  server_->Wait();
  return;
}

void GTStoreStorage::initialize() {
	grpc::EnableDefaultHealthCheckService(true);
	grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  	ServerBuilder builder;
  	builder.AddListeningPort(server_address_, grpc::InsecureServerCredentials());
  	builder.RegisterService(service_.get());
  	server_ = builder.BuildAndStart();
	return;
}
void GTStoreStorage::liveness(){
	NodeAddr request;
	request.set_addr(server_address_);
	Empty response;
	while(true){
		ClientContext context;
		Status status = manager_stub_->liveness(&context, request, &response);
		if(status.ok()){
			sleep(2);
		}
		else{
			return;
		}
	}
}
void GTStoreStorage::handshake(){
	//cout << "Updating node array..." << endl;
  	ClientRequest request;
  	request.set_user_id(id_);
  	ManagerReply response;
  	ClientContext context;
	Status status = manager_stub_->Handshake(&context, request, &response);
  	std::map<uint32_t, std::string> new_nodes;
    	if (status.ok()) {
			new_nodes = std::map<uint32_t, std::string>(
            	response.nodes().begin(), 
            	response.nodes().end()
        	);
        	system_state_->SetNodes(new_nodes); 
			//adjust k so that we don't try to replicate to more nodes than we have and spin in an infinite loop.
			int state_size = (int) system_state_->Size() / n_;
			if(state_size  < k_){
				k_ = state_size;
			}
		/*map<uint32_t, string>::iterator it;
		for (it = system_state_->nodes.begin(); it != system_state_->nodes.end(); it++){
    		std::cout << it->first   << " : " << it->second   << std::endl;
  		}*/
		//std::cout << "Number of virtual nodes: " << system_state_->Size() << endl;
		return;
  	} 
  	else {
      	std::cout  << status.error_code() << ": " << status.error_message() << std::endl;
      	return;
  	}
	
}
void GTStoreStorage::setup(){
	std::thread worker1(std::mem_fn(&GTStoreStorage::wait), this);
	//may need more threads than this - backlog will get k times more requests than the listening port.
	//std::thread worker2(std::mem_fn(&GTStoreStorage::handle_backlog), this);
	//Have one thread that is solely responsible for providing the manager with a liveness check every so often.
	std::thread worker3(std::mem_fn(&GTStoreStorage::liveness), this);
	worker1.join();
    //worker2.join();
	worker3.join();
	return;
}
//storage will need an address configured by the manager, knowledge of the replication parameter, an id, and the manager's id
int main(int argc, char **argv){
	if (argc != 11){
		cout << "Usage: ./storage -m MANAGER_ADDRESS -a PORT_ADDRESS --id NODE_ID -k REPLICATION_PARAMETER";
		return 0;
	}
	char* manager_port = argv[2];
	char* port = argv[4];
	int id = stoi(argv[6]);
	int k = stoi(argv[8]);
	int n = stoi(argv[10]);
	cout << "Initializing storage server on port: "  << port  << " with id " << id << " and PID: " << getpid() << endl;
	GTStoreStorage node(grpc::CreateChannel(manager_port, grpc::InsecureChannelCredentials()), std::string(port), id, k, n);
	//contact manager and receive the current node mappings.
	node.handshake();
	//setup server for contact 
	node.initialize();
	//Create this nodes thread pool - multiple threads waiting on the server, one thread checking the backlog 
	//and performing RPC put calls as necessary. Might need to revise this setup - shouldn't be too hard.
	node.setup();
	//node.wait();
	return 1 ;
}
