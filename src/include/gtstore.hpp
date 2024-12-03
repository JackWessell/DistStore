#ifndef GTSTORE
#define GTSTORE

#include <mutex>
#include <map>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <thread> 

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/grpcpp.h>
#include "demo.grpc.pb.h"
#include "gtnode.hpp"
#include "gthash.hpp"

using grpc::Channel;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
//For Manager Node
using demo::Manager;
using demo::ClientRequest;
using demo::ManagerReply;
using demo::NodeAddr;
using demo::Empty;
//For storage Node:
using demo::Storage;
using demo::ClientKey;
using demo::ClientValue;
using demo::Ack;
using demo::KeyValue;

#define MAX_KEY_BYTE_PER_REQUEST 20
#define MAX_VALUE_BYTE_PER_REQUEST 1000

using namespace std;

typedef vector<string> val_t;

//Client will not need to initialize any RPC calls as it simply calls the functions defined by manager and storage nodes
class GTStoreClient {
	private:
		int client_id;
		val_t value;
		std::unique_ptr<Manager::Stub> manager_stub;
		std::unique_ptr<std::map<uint32_t, string>> storage_nodes;
	public:
		GTStoreClient(std::shared_ptr<Channel> channel)
    		: manager_stub(Manager::NewStub(channel))
			, storage_nodes(std::make_unique<std::map<uint32_t, string>>())
  		{}
		void init(int id);
		void get_map();
		void finalize();
		//change to use string key and val_t value soon - just want to get core functionality down first.
		std::string get(string addr, uint32_t key);
		int put(string addr, uint32_t key, string value);
		std::string get_node(uint32_t key);
		void handshake();
};


class ManagerImpl final : public Manager::Service {
	private:
    	std::shared_ptr<GTNodeData> state_;
		std::shared_ptr<TimeArray> heartbeat_;
	public:
    	explicit ManagerImpl(const std::shared_ptr<GTNodeData>& state, const std::shared_ptr<TimeArray>& heartbeat)
        	: state_(state) 
			, heartbeat_(heartbeat)
		{}
		Status Handshake(
			ServerContext* context,
        	const ClientRequest* request,
        	ManagerReply* response 
		);
		Status liveness(
			ServerContext* context,
			const NodeAddr* request,
			Empty* response
		);

};

//This is the primary class for the server. It contains the server, storage nodes, and service containing the RPC calls
class GTStoreManager{
	private:
		//will have hashes as keys and addresses as values
		std::unique_ptr<Server> server_;
		std::shared_ptr<TimeArray> heartbeat_;
		std::shared_ptr<GTNodeData> state_;
    	std::unique_ptr<ManagerImpl> service_;
    	std::string server_address_;
		
	public:
		GTStoreManager() 
			: state_(std::make_shared<GTNodeData>())
			, heartbeat_(std::make_shared<TimeArray>())
			, service_(std::make_unique<ManagerImpl>(state_, heartbeat_))
			, server_(nullptr)
		{}
		void handle_sigterm();
		void setup_signal_handler();
		void add_node(uint32_t key, const std::string& server_address);
		void start_liveness(const std::string& addr);
		void check_liveness();
		void initialize(const std::string& server_address);
		void shutdown();
		void setup();
		void wait();
};
class StorageCallBack{
	public:
		virtual void NodeFailure() = 0;
};
//storage nodes should have a very similar structure to the manager node: a central class, a server class, and sharing a key-value table.
class StorageImpl final : public Storage::Service{
	private:
		std::shared_ptr<GTNodeData> system_state_;
		std::shared_ptr<GTNodeData> storage_state_;
		std::string addr_;
		StorageCallBack* callback_;
		int id_;
		int k_;
		int n_;
	public:
		explicit StorageImpl(const std::shared_ptr<GTNodeData>& system_state, 
							 const std::shared_ptr<GTNodeData>& storage_state,
							 const std::string& addr,
							 int id,
							 int k,
							 int n,
							 StorageCallBack* callback)
			: system_state_(system_state) 
			, storage_state_(storage_state)
			, addr_(addr)
			, id_(id)
			, k_(k)
			, n_(n)
			, callback_(callback)
		{}
		bool replicate(std::string addr, uint32_t key, std::string val);
		void put_helper(uint32_t key, std::string val);
		Status put(
			ServerContext* context,
			const KeyValue* request,
			Ack* response
		);
		Status get(
			ServerContext* context,
			const ClientKey* request,
			ClientValue* response
		);
};
class GTStoreStorage : public StorageCallBack{
	private:
		std::string server_address_;
		int id_;
		int k_;
		int n_;
		//data that can be stored with the system handling RPC calls
		std::shared_ptr<GTNodeData> system_state_;
		std::shared_ptr<GTNodeData> storage_state_;

		//information that is unique to the storage object
		std::unique_ptr<Server> server_;
		std::unique_ptr<StorageImpl> service_;
		std::unique_ptr<Manager::Stub> manager_stub_;

	public:
		GTStoreStorage(std::shared_ptr<Channel> channel, std::string server, int id, int k, int n)
			: server_address_(server)
			, id_(id)
			, k_(k)
			, n_(n)
			, system_state_(std::make_shared<GTNodeData>())
			, storage_state_(std::make_shared<GTNodeData>())
			, service_(std::make_unique<StorageImpl>(system_state_, storage_state_, server_address_, id_, k_, n_, this))
			, server_(nullptr)
			, manager_stub_(Manager::NewStub(channel))
		{}
		void NodeFailure() override{
			handshake();
		}
		void initialize();
		void wait();
		void setup();
		void handle_backlog();
		void handshake();
		void liveness();
};

#endif
