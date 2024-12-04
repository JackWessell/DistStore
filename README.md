# Distributed key-value store with gRPC

In this project I developed a distributed key-value store that allows clients to interact with a system consisting of a manager node and n storage nodes to store key-value pairs. To facilitate communication between nodes, I utilized Google's gRPC library. First I will describe how to install necessary libraries and then how to run my code.

## Dependencies
### Install cmake locally without sudo
Install cmake locally:
```shell
wget https://github.com/Kitware/CMake/releases/download/v3.23.0/cmake-3.23.0-linux-x86_64.sh
mkdir -p $HOME/localcmake 
sh cmake-3.23.0-linux-x86_64.sh --prefix=$HOME/localcmake --skip-license 
export PATH=$HOME/localcmake/bin:$PATH #You need to do this every time you login, otherwise put in $HOME/.profile 
which cmake # /home/<gtid>/localcmake/bin/cmake
cmake --version # cmake version 3.23.0
```

### Installing Dependencies with sudo

Install cmake with sudo:
```shell
sudo apt-get install cmake
cmake --version # output should be >= 3.15
```

Dependencies to build gRPC:
```shell
sudo apt-get install build-essential autoconf libtool pkg-config
```

ninja-build is an optional dependency that can speed up the gRPC build.
```shell
sudo apt-get install ninja-build # optional dependency
ninja --version #Check install
```

## Building the Demo
```shell
https://github.com/JackWessell/DistStore.git
cd DistStore
cmake -S . -B ./build #-G Ninja (optional: add this if you installed ninja-build as it will decrease build time)
cmake --build ./build --target manager client storage test_app
```
Note: This process is a bit slow the first time around. 

## Running Server & Client
In one terminal:
```shell
cd build
./src/manager -n NUMBER_OF_STORAGE_NODES -k REPLICATION_PARAM -a ADDRESS
```
This will create a manager node which will automatically spawn n child processes to act as storage nodes. Currently, the application only accepts localhost addresses that do not start with a zero. So, the smallest valid address is 0.0.0.0:10000. 

In a different terminal, to put values:
```shell
cd build
./src/client -a ADDRESS --put KEY --val VALUE
```
And to get values:
```shell
cd build
./src/client -a ADDRESS --get KEY
```
The address for both of these calls should be the same that you used to spawn the manager. The client API is capable of mapping string keys to string values.
## Running tests
The repository also contains a bash script for testing the behavior of my code. It tests a single server put/get, multi-server put/get with over-writing, a multi-server, single node failure put/get, and a multi-server, multi-node failure put/get. To run these tests, simply do the following:
```shell
cd ..
./src/run.sh ADDRESS
```
A note on these tests: for the node failure tests, the client programs will eventually try to contact a dead node. They will hang until the manager realizes these nodes are dead and updates the system state. The code has not failed - just wait a little and progress should pick back up.

