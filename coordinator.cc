#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "coordinator.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::CoordService;
using csce438::ServerInfo;
using csce438::Confirmation;
using csce438::ID;
// using csce438::Status
// using csce438::ServerList;
// using csce438::SynchService;

std::time_t getTimeNow(){
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

struct zNode{
    int serverID;
    std::string hostname;
    std::string port;
    std::string type;
    std::time_t last_heartbeat;
    bool missed_heartbeat;
    bool isActive();

};

bool zNode::isActive(){
    bool status = false;
    // if(!missed_heartbeat){
    //     status = true;
    // }else 
    if(difftime(getTimeNow(),last_heartbeat) < 10){
        status = true;
    }
    std::cout<<serverID<<" "<<status<<std::endl;
    return status;
}

//potentially thread safe
std::mutex v_mutex;
std::vector<zNode> cluster1;
std::vector<zNode> cluster2;
std::vector<zNode> cluster3;


//func declarations
int findServer(std::vector<zNode> v, int id);
std::time_t getTimeNow();
void checkHeartbeat();

class CoordServiceImpl final : public CoordService::Service {

  
  Status Heartbeat(ServerContext* context, const ServerInfo* serverinfo, Confirmation* confirmation) override {
    //std::cout<<"Got Heartbeat! "<<serverinfo->type()<<"("<<serverinfo->serverid()<<")"<<std::endl;

    // Your code here
    int id=serverinfo->serverid();
    if(id==1){
      cluster1[cluster1.size()-1].last_heartbeat=getTimeNow();
    }
    if(id==2){
      cluster2[cluster2.size()-1].last_heartbeat=getTimeNow();
    }
    if(id==3){
      cluster3[cluster3.size()-1].last_heartbeat=getTimeNow();
    }

    confirmation->set_status(true);
    return Status::OK;
  }
  
  //function returns the server information for requested client id
  //this function assumes there are always 3 clusters and has math
  //hardcoded to represent this.
  Status GetServer(ServerContext* context, const ID* id, ServerInfo* serverinfo) override {
    //std::cout<<"Got GetServer for clientID: "<<id->id()<<std::endl;
    int serverID = (id->id()%3)+1;

    // Your code here
    // If server is active, return serverinfo
    int idf = id->id();
    serverinfo->set_type("down");

    if(idf==1){
      serverinfo->set_clusterid((int32_t)1);
      serverinfo->set_serverid((int32_t)idf);
      serverinfo->set_hostname(cluster1[cluster1.size()-1].hostname);
      serverinfo->set_port(cluster1[cluster1.size()-1].port);
      if(cluster1[cluster1.size()-1].isActive()) serverinfo->set_type("alive");
    }
    else if(idf==2){
      serverinfo->set_clusterid((int32_t)2);
      serverinfo->set_serverid((int32_t)idf);
      serverinfo->set_hostname(cluster2[cluster2.size()-1].hostname);
      serverinfo->set_port(cluster2[cluster2.size()-1].port);
      if(cluster2[cluster2.size()-1].isActive()) serverinfo->set_type("alive");
    }
    else if(idf==3){
      serverinfo->set_clusterid((int32_t)3);
      serverinfo->set_serverid((int32_t)idf);
      serverinfo->set_hostname(cluster2[cluster2.size()-1].hostname);
      serverinfo->set_port(cluster2[cluster2.size()-1].port);
      if(cluster3[cluster3.size()-1].isActive()) serverinfo->set_type("alive");
    }

    return Status::OK;
  }

  Status Create(ServerContext* context, const ServerInfo* serverinfo, Confirmation* confirmation) override {
    zNode node=*(new zNode());

    node.hostname=serverinfo->hostname();
    node.last_heartbeat=getTimeNow();
    node.serverID=serverinfo->serverid();
    node.port=serverinfo->port();

    int cid=serverinfo->clusterid();
    if(cid==1){
      cluster1.push_back(node);
    }
    if(cid==2){
      cluster2.push_back(node);
    }
    if(cid==3){
      cluster3.push_back(node);
    }
    
    return Status::OK;
  }
  

};

void RunServer(std::string port_no){
  //start thread to check heartbeats
  std::thread hb(checkHeartbeat);
  //localhost = 127.0.0.1
  std::string server_address("127.0.0.1:"+port_no);
  CoordServiceImpl service;
  //grpc::EnableDefaultHealthCheckService(true);
  //grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      default:
	std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunServer(port);
  return 0;
}



void checkHeartbeat(){
  while(true){
      //check servers for heartbeat > 10
      //if true turn missed heartbeat = true
      // Your code below
      std::vector<zNode> servers;
      if(cluster1.size()!=0){
        servers.push_back(
          cluster1[cluster1.size()-1]
        );
      }
      if(cluster2.size()!=0){
        servers.push_back(
          cluster2[cluster2.size()-1]
        );
      }
      if(cluster3.size()!=0){
        servers.push_back(
          cluster3[cluster3.size()-1]
        );
      }

      for(auto& s : servers){
        if(difftime(getTimeNow(),s.last_heartbeat)>10){
          std::cout<<"check "<<s.serverID<<" is down"<<std::endl;
          s.missed_heartbeat = true;
          s.type="down";
          // if(!s.missed_heartbeat){
          //   s.missed_heartbeat = true;
          //   s.last_heartbeat = getTimeNow();
          // }else{
          //   s.type="down";
          // }
        }
      }
      
      sleep(1);
    }
}


