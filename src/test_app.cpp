#include <signal.h>
#include <regex>
#include <fcntl.h>
#include <sys/wait.h>
#include <fstream> 
#include <ext/stdio_filebuf.h>

#include "gtstore.hpp"
#include "gtnode.hpp"

class ClientLog{
	public:
		int key;
		string value;
		int node;
		void printput(){
			cout << "Used Key " << key  << " to place \'" << value << "\' at node " << node << endl;
			return;
		}
		void printrec(){
			cout << "Used Key " << key  << " to get \'" << value << "\' from node " << node << endl;
			return;
		}
};
class ManagerLog{
	public:
		std::atomic<bool> end_test{false};
		//this map maps addresses to their node ids.
		std::map<string, int> addresses;
		//this map contains mappings from node ids to their PIDs.
		std::map<int, int> active;
		//this map contains nodes that have died.
		std::set<int> dead;
};
void client_put(ClientLog *log, std::string addr, int key, string value){
	int pipefd[2];
    pid_t pid;
    char buffer[1024];
	//setup pipe
	if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

	pid = fork();
	if(pid == 0){
		std::vector<char*> argv;
		string key_str = to_string(key);
		argv.push_back("./build/src/client");
		argv.push_back("--a");
		argv.push_back(addr.data());
		argv.push_back("--put");
		argv.push_back(key_str.data());
		argv.push_back("--val");
		argv.push_back(value.data());
		argv.push_back(nullptr);

		close(pipefd[0]); 
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

		execv(argv[0], argv.data());
		std::cerr << "Exec failed: " << strerror(errno) << std::endl;
     	 _exit(EXIT_FAILURE);
	}
	close(pipefd[1]);
    std::vector<char> output;
    ssize_t bytesRead;
    while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        output.insert(output.end(), buffer, buffer + bytesRead);
    }
    close(pipefd[0]);
	int status;
	waitpid(pid, &status, 0);
	const string res(output.begin(), output.end());
	//response from puts follow an exact pattern. Use regex to extract data.
	std::regex rgx(R"(Successfully put: '([^']+)' onto node: (\d+) with key: '([^']+)')");
	std::smatch matches;
	if(std::regex_search(res.begin(), res.end(), matches, rgx)){
		log->key = stoi(matches[3].str().data());
		log->node = stoi(matches[2].str().data());
		log->value = matches[1];
		log->printput();
	}
	
	else{
		cout << res << endl;
	}
	return;
}
void client_get(ClientLog* log, std::string addr, int key){
	int pipefd[2];
    pid_t pid;
    char buffer[1024];
	//setup pipe
	if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

	pid = fork();
	if(pid == 0){
		std::vector<char*> argv;
		string key_str = to_string(key);
		argv.push_back("./build/src/client");
		argv.push_back("--a");
		argv.push_back(addr.data());
		argv.push_back("--get");
		argv.push_back(key_str.data());
		argv.push_back(nullptr);

		close(pipefd[0]); 
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

		execv(argv[0], argv.data());
		std::cerr << "Exec failed: " << strerror(errno) << std::endl;
      	_exit(EXIT_FAILURE);
	}
	close(pipefd[1]);
    std::vector<char> output;
    ssize_t bytesRead;
    while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        output.insert(output.end(), buffer, buffer + bytesRead);
    }
    close(pipefd[0]);
	int status;
	waitpid(pid, &status, 0);
	const string res(output.begin(), output.end());
	std::regex rgx(R"(Received '([^']+)' using key '([^']+)' from node (\d+))");
	std::smatch matches;
	if(std::regex_search(res.begin(), res.end(), matches, rgx)){
		log->key = stoi(matches[2].str().data());
		log->node = stoi(matches[3].str().data());
		log->value = matches[1];
		log->printrec();
	}
	else{
		cout << res << endl;
	}
	return;
}
void manager_setup(ManagerLog* log, std::string addr, int n, int k){
	int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("pipe creation failed");
        return;
    }
	pid_t pid = fork();
	if(pid == 0){
		close(pipe_fds[0]);  // Close read end in child
		dup2(pipe_fds[1], STDOUT_FILENO);
        close(pipe_fds[1]);

		string n_str = to_string(n);
		string k_str = to_string(k);
		std::vector<char*> argv;
		argv.push_back("./build/src/manager");
		argv.push_back("-n");
		argv.push_back(n_str.data());
		argv.push_back("-k");
		argv.push_back(k_str.data());
		argv.push_back("-a");
		argv.push_back(addr.data());
		argv.push_back(nullptr);
		execv(argv[0], argv.data());

		std::cerr << "Exec failed: " << strerror(errno) << std::endl;
      	_exit(EXIT_FAILURE);
	}
	close(pipe_fds[1]);
	int flags = fcntl(pipe_fds[0], F_GETFL, 0);
    fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);
	__gnu_cxx::stdio_filebuf<char> filebuf(pipe_fds[0], std::ios::in);
	filebuf.pubsetbuf(0, 0);
	// Create an istream that uses this buffer
	std::istream input_stream(&filebuf);
	std::string line;
	while (!log->end_test){
		if (!(input_stream.rdbuf()->in_avail())) continue;
        if (std::getline(input_stream, line)) {
            // We have a complete line - process it
			std::cout << "Received: " << line << std::endl;
			if(line.find("Manager") != std::string::npos){
				continue;
			}
			else if(line.find("Initializing") != std::string::npos){
				std::regex rgx(R"(Initializing storage server on port: (.+) with id (\d+) and PID: (\d+))");
				std::smatch matches;
				std::regex_search(line, matches, rgx);
				log->addresses[matches[1].str()] = stoi(matches[2].str().data());
				log->active[stoi(matches[2].str().data())] = stoi(matches[3].str().data());
			}
			else if(line.find("dead") != std::string::npos){
				std::regex rgx(R"(Node at (.+) is dead)");
				std::smatch matches;
				std::regex_search(line, matches, rgx);
				log->dead.insert(log->addresses[matches[1].str()]);
			}
			else{
				continue;
			}
        }
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
	kill(pid, SIGTERM);
	return;
}

void single_set_get(std::string addr){
	//set-up node with one storage server
	ManagerLog mlog;
	std::thread worker(manager_setup, &mlog, addr, 1, 1);
	//wait until manager has been created.
	ClientLog log;
	while(mlog.active.size() == 0){
		sleep(.1);
		continue;
	}
	client_put(&log, addr, 0, "Hello");
	client_put(&log, addr,1, "GT");
	client_put(&log, addr,2, "Store!");
	client_get(&log, addr,0);
	client_get(&log, addr,1);
	client_get(&log, addr,2);
	
	mlog.end_test = true;
	worker.join();
	return;
}

void multi_set_get(std::string addr){
	ManagerLog mlog;
	std::thread worker(manager_setup, &mlog,addr, 5, 3);
	//Wait to allow manager time to setup.
	ClientLog log;
	while(mlog.active.size() != 5){
		sleep(.1);
		continue;
	}
	client_put(&log, addr,10, "Hello");
	client_put(&log, addr,11, "GT");
	client_put(&log,addr,22, "Store!");
	client_put(&log,addr,36, "I have");
	client_put(&log,addr,47, "many storage");
	client_put(&log,addr,509, "Nodes. Cool!");
	client_put(&log,addr,36, "Be Careful when over-writing!!!");
	client_get(&log,addr,10);
	client_get(&log,addr,11);
	client_get(&log,addr,22);
	client_get(&log,addr,36);
	client_get(&log,addr,47);
	client_get(&log,addr,509);
	mlog.end_test = true;
	worker.join();
	return;
}
void single_node_fail(std::string addr){
	std::srand(std::time(0)); 
	ManagerLog mlog;
	std::thread worker(manager_setup, &mlog,addr, 3, 2);
	//wait until manager has been created.
	ClientLog log;
	while(mlog.active.size() != 3){
		sleep(.1);
		continue;
	}
	int min = 0;
    int max = 1000;
    
	std::map<int, int> count;
	std::map<int, vector<int>> keys;
	//by pigeonhole principle, after 7 iterations one server must have at least 3.
	std::vector<string> strings = {"One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten"};
	int curr;
	int num;
	for( int i = 0; i < 10; i++){
		num = min + std::rand() % (max - min + 1); 
		client_put(&log,addr, num, strings[i]);
		curr = log.node;
		count[curr]++;
		keys[curr].push_back(num);
		if(count[curr] == 4) break;
	}
	cout << "Overwriting..." << endl;
	client_put(&log, addr,num, "Overwrite!!");
	//kill the node with the most keys.
	kill(mlog.active[curr], SIGKILL);
	for(int key : keys[curr]){
		client_get(&log,addr, key);
	}
	mlog.end_test = true;
	worker.join();
}
void multi_node_fail(std::string addr){
	std::srand(std::time(0)); 
	ManagerLog mlog;
	std::thread worker(manager_setup, &mlog, addr,7, 3);
	//wait until manager has been created.
	ClientLog log;
	while(mlog.active.size() != 7){
		sleep(.1);
		continue;
	}
	int min = 0;
    int max = 2000;
	std::map<int, int> count;
	std::map<int, vector<int>> keys;
	std::vector<int> rands;
	//by pigeonhole principle, after 7 iterations one server must have at least 3.
	std::vector<string> strings = {"One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten",
						"Eleven", "Twelve", "Thirteen", "Fourteen", "Fiveteen", "Sixteen", "Seventeen", "Eightteen", "Nineteen", "Twenty"};
	int curr;
	int num;
	for( int i = 0; i < 20; i++){
		num = min + std::rand() % (max - min + 1); 
		client_put(&log, addr,num, strings[i]);
		curr = log.node;
		count[curr]++;
		keys[curr].push_back(num);
		rands.push_back(num);
	}
	max = 20;
	cout << "Overwriting..." << endl;
	num = min + std::rand() % (max - min + 1); 
	client_put(&log, addr,rands[num], "Overwrite 1!!!");
	num = min + std::rand() % (max - min + 1); 
	client_put(&log, addr,rands[num], "Overwrite 2!!!");
	num = min + std::rand() % (max - min + 1); 
	client_put(&log, addr,rands[num], "Overwrite 3!!!");
	//kill the node with the most keys.
	max = 6;
	num = min + std::rand() % (max - min + 1); 
	cout << "Killing..." << endl;
	cout << "Killing node " << num << endl;
	kill(mlog.active[num], SIGKILL);
	int new_num = min + std::rand() % (max - min + 1); 
	while(new_num == num) new_num =  min + std::rand() % (max - min + 1); 
	cout << "Killing node " << new_num << endl;
	kill(mlog.active[new_num], SIGKILL);
	for(int key : rands){
		client_get(&log, addr,key);
	}
	mlog.end_test = true;
	worker.join();
}
// IMPORTANT!!!! Before running, comment/uncomment code in client.cpp. I did not fully implement as I ran throughput/load balancing tests only once.
void throughput(std::string addr){
	std::srand(std::time(0)); 
	ManagerLog mlog;
	std::thread worker(manager_setup, &mlog, addr,7, 5);
	//wait until manager has been created.
	ClientLog log;
	while(mlog.active.size() != 7){
		sleep(.1);
		continue;
	}
	pid_t pids[10]; 
	TimePoint then = Clock::now();
	for(int i = 0; i < 10; i++){
		pid_t pid = fork();
		if(pid == 0){
			std::vector<char*> argv;
			string start_string = to_string(i*20000);
			argv.push_back("./build/src/client");
			//argv.push_back("--get");
			argv.push_back(start_string.data());
			argv.push_back(nullptr);

			execv(argv[0], argv.data());
		}
		else{
			pids[i] = pid;
		}
	}
	int status;
	for(int i = 0; i < 10; i++){
		waitpid(pids[i], &status, 0);
	}
	TimePoint now = Clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - then
    ).count();
	cout << "Time for 200000 requests: " << elapsed << endl;
	mlog.end_test = true;
	worker.join();
	return;
}
int main(int argc, char **argv) {
	string test = string(argv[1]);
	string addr = string(argv[2]);
	string test1 = "single_set_get";
	string test2 = "multi_set_get";
	string test3 = "single_node_fail";
	string test4 = "multi_node_fail";
	string test5 = "throughput";
	if (test ==  test1) {
		single_set_get(addr);
	}
	if (test ==  test2) {
		multi_set_get(addr);
	}
	if(test==test3){
		single_node_fail(addr);
	}
	if(test == test4){
		multi_node_fail(addr);
	}
	if(test == test5){
		cout << "Not implemented..." << endl;
		//throughput(); - not fully implemented - currently need to edit client.cpp
	}
	return 1;
}