#ifndef GTNODE
#define GTNODE
#include <iostream>
#include <mutex>
#include <map>
#include <string>
#include <random>
#include <cstdint>
#include <chrono> 

using namespace std;
using Clock = std::chrono::system_clock; 
using TimePoint = std::chrono::time_point<Clock>;

//This is a class allowing for thread-safe access to the node list.
//This is necessary as both our Manager and RPC calls need access to the nodes. Will use a shared pointer.
class GTNodeData{
	private:
		std::mutex data_mutex_;
		std::map<uint32_t, std::string> nodes;
	public:
		void AddNode(uint32_t key, std::string value);
        void RemoveValue(std::string value);
        void SetNodes(std::map<uint32_t, std::string> new_map);
        int Size();
		std::string Get(uint32_t key);
		std::map<uint32_t, std::string> GetNodes();


    friend class GTStoreStorage;
    friend class StorageImpl;
};
//This class handles timing. It allows the manager to heuristically keep track of live nodes in the system.
class TimeArray{
    private:
        std::mutex data_mutex_;
        std::map<std::string, TimePoint> info_;
    public:
        TimePoint GetTime(std::string node);
        void AddNode(std::string addr, TimePoint time);
    friend class GTStoreManager;
};
//A class that stores all information necessary to determine where to send a replicated key-value.
class DataReplicate{
    public:
        std::string addr;
        int key;
        std::string val;
        DataReplicate(std::string address, int key_, std::string value)
            : addr(address)
            , key(key_)
            , val(value)
        {}
};
//a thread-safe backlog that stores all data that is yet to be replicated
class Backlog{
    private:
        std::mutex data_mutex_;
        std::vector<DataReplicate> backlog_;
    public:
        int get_size();
        DataReplicate pop();
        void push(DataReplicate data);

};

//A random number generator class that is used to assign values to nodes - this will allow for the hash-based load balancing scheme suggested by Dynamo.
class RandomGenerator {
private:
    std::random_device rd;
    std::mt19937 generator;
    std::uniform_int_distribution<uint32_t> distribution;

public:
    RandomGenerator(int seed) 
        : generator(seed)
        , distribution(0, UINT32_MAX) 
    {}

    uint32_t get_random();
};
#endif