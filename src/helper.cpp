#include "gtnode.hpp"

//GTNode functions
void GTNodeData::AddNode(uint32_t key, std::string value){
    std::lock_guard<std::mutex> lock(data_mutex_);
    nodes[key] = value;
    return;
}
std::string GTNodeData::Get(uint32_t key){
    std::lock_guard<std::mutex> lock(data_mutex_);
    if(nodes.find(key) == nodes.end()){
        //place-holder value - will change when system is ready for submission (new data class created reflective of the project's criteria)
        return "///";
    }
    return nodes[key];
}
std::map<uint32_t, std::string> GTNodeData::GetNodes(){
    std::lock_guard<std::mutex> lock(data_mutex_);
    return nodes;
}
void GTNodeData::RemoveValue(std::string value){
    map<uint32_t, std::string>::iterator it;
    std::vector<uint32_t> dropping;
    std::lock_guard<std::mutex> lock(data_mutex_);
    for(it = nodes.begin(); it != nodes.end(); it++){
        if(it->second == value){
            dropping.push_back(it->first);
        }
    }
    for(uint32_t key : dropping){
        nodes.erase(key);
    }
    return;
}
void GTNodeData::SetNodes(std::map<uint32_t, std::string> new_nodes){
    std::lock_guard<std::mutex> lock(data_mutex_);
    nodes = new_nodes;
    return;
}
int GTNodeData::Size(){
    std::lock_guard<std::mutex> lock(data_mutex_);
    return nodes.size();
}
//Backlog functions.
void Backlog::push(DataReplicate data){
    std::lock_guard<std::mutex> lock(data_mutex_);
    backlog_.push_back(data);
    return;
}
DataReplicate Backlog::pop(){
    std::lock_guard<std::mutex> lock(data_mutex_);
    DataReplicate obj = backlog_.front();
    backlog_.erase(backlog_.begin());
    return obj;
}
int Backlog::get_size(){
    std::lock_guard<std::mutex> lock(data_mutex_);
    return backlog_.size();
}
//TimeArray functions:
TimePoint TimeArray::GetTime(std::string node){
    std::lock_guard<std::mutex> lock(data_mutex_);
    return info_[node];
}
void TimeArray::AddNode(std::string addr, TimePoint time){
    std::lock_guard<std::mutex> lock(data_mutex_);
    info_[addr] = time;
    return;
}

uint32_t RandomGenerator::get_random() {
    return distribution(generator);
}