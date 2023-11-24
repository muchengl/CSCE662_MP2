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
using csce438::GetSlaveRequset;
using csce438::ID;
using csce438::GetAllServersRequset;
using csce438::GetAllServersResponse;

std::time_t getTimeNow(){
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

struct zNode{
    int serverID;
    int clusterID;
    std::string hostname;
    std::string port;
    std::string type; // down | alive
    std::time_t last_heartbeat;

    // master | slave | synchronizer
    bool isMaster;
    bool isSynchronizer;

    bool missed_heartbeat;
    bool isActive();
    bool isServer();

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

// is a server, not a synchronizer
bool zNode::isServer(){
    return !isSynchronizer;
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
    //std::cout<<"Got Heartbeat! "<<"("<<serverinfo->clusterid()<<") - ("<<serverinfo->serverid()<<")"<<std::endl;
    // serverinfo.
    // Your code here
    v_mutex.lock();

    int id=serverinfo->clusterid();
    int serverId=serverinfo->serverid();
    if(id==1){
      for(int i=0;i<cluster1.size();i++){
        if(serverId == cluster1[i].serverID){
          cluster1[i].last_heartbeat=getTimeNow();
          break;
        }
      }
    }
    if(id==2){
      for(int i=0;i<cluster2.size();i++){
        if(serverId == cluster2[i].serverID){
          cluster2[i].last_heartbeat=getTimeNow();
          break;
        }
      }
    }
    if(id==3){
      for(int i=0;i<cluster3.size();i++){
        if(serverId == cluster3[i].serverID){
          cluster3[i].last_heartbeat=getTimeNow();
          break;
        }
      }
    }

    confirmation->set_status(true);

     v_mutex.unlock();
    return Status::OK;
  }
  
  void init_serverinfo(ServerInfo* serverinfo,zNode &node,int cid){
    serverinfo->set_clusterid((int32_t)cid);
    serverinfo->set_serverid((int32_t)node.serverID);
    serverinfo->set_hostname(node.hostname);
    serverinfo->set_port(node.port);
    serverinfo->set_type("alive");
    if(node.isSynchronizer) serverinfo->set_servertype("synchronizer");
    else serverinfo->set_servertype("server");
    
  }

  //function returns the server information for requested client id
  //this function assumes there are always 3 clusters and has math
  //hardcoded to represent this.
  Status GetServer(ServerContext* context, const ID* id, ServerInfo* serverinfo) override {
    //std::cout<<"Got GetServer for clientID: "<<id->id()<<std::endl;
    int userID = id->id();
    int clusterID = ((userID-1)%3)+1;

    // int userID = id->id();
    serverinfo->set_type("down");

    if(clusterID==1){
      bool find=false;
      for(int i=0;i<cluster1.size();i++){
        if(cluster1[i].isMaster){
          std::cout<<"Find master, cluster1 - server"<<cluster1[i].serverID<<std::endl;
          init_serverinfo(serverinfo,cluster1[i],clusterID);

          find=true;
          break;
        }
      }
      if(!find)
      for(int i=0;i<cluster1.size();i++){
        if(cluster1[i].type == "alive" && cluster1[i].isServer()){
          std::cout<<"New master, cluster1 - server"<<cluster1[i].serverID<<std::endl;
          cluster1[i].isMaster=true;

          init_serverinfo(serverinfo,cluster1[i],clusterID);

          break;
        }
      }
    }

    else if(clusterID==2){
      bool find=false;
      for(int i=0;i<cluster2.size();i++){
        if(cluster2[i].isMaster){
          std::cout<<"Find master, cluster2 - server"<<cluster2[i].serverID<<std::endl;

          init_serverinfo(serverinfo,cluster2[i],clusterID);

          find=true;
          break;
        }
      }
      if(!find)
      for(int i=0;i<cluster2.size();i++){
        if(cluster2[i].type == "alive" && cluster2[i].isServer()){
          std::cout<<"New master, cluster2 - server"<<cluster2[i].serverID<<std::endl;
          cluster2[i].isMaster=true;

          init_serverinfo(serverinfo,cluster2[i],clusterID);

          break;
        }
      }
    }

    else if(clusterID==3){
      bool find=false;
      for(int i=0;i<cluster3.size();i++){
        if(cluster3[i].isMaster){
          std::cout<<"Find master, cluster3 - server"<<cluster3[i].serverID<<std::endl;

          init_serverinfo(serverinfo,cluster3[i],clusterID);

          find=true;
          break;
        }
      }
      if(!find)
      for(int i=0;i<cluster3.size();i++){
        if(cluster3[i].type == "alive" && cluster3[i].isServer()){
          std::cout<<"New master, cluster3 - server"<<cluster3[i].serverID<<std::endl;
          cluster3[i].isMaster=true;

          init_serverinfo(serverinfo,cluster3[i],clusterID);
          break;
        }
      }
    }

    return Status::OK;
  }

  // When a server start, it will call this function to regisiter itself to coordinator
  Status Create(ServerContext* context, const ServerInfo* serverinfo, Confirmation* confirmation) override {
    //std::cout<<"Got Create! "<<"("<<serverinfo->clusterid()<<") - ("<<serverinfo->serverid()<<")"<<std::endl;
    zNode node=*(new zNode());

    node.hostname=serverinfo->hostname();
    node.last_heartbeat=getTimeNow();
    node.serverID=serverinfo->serverid();
    node.clusterID=serverinfo->clusterid();

    node.port=serverinfo->port();
    node.type="alive";

    if(serverinfo->servertype()=="synchronizer" || serverinfo->servertype()!="server"){
      std::cout<<"New Synchronizer "<<"("<<serverinfo->clusterid()<<") - ("<<serverinfo->serverid()<<")"<<std::endl;
      node.isSynchronizer=true;
    }

    int cid=serverinfo->clusterid();
    // Cluster 1
    if(cid==1){
      bool find=false;
      for(int i=0;i<cluster1.size();i++){
        if(cluster1[i].serverID == node.serverID && cluster1[i].isSynchronizer==node.isSynchronizer){
          std::cout<<"ReNew Node"<<"("<<serverinfo->clusterid()<<") - ("<<serverinfo->serverid()<<")"<<std::endl;
          node.type = "alive";
          find=true;
          break;
        }
      }
      if(!find) {
        std::cout<<"New Node"<<"("<<serverinfo->clusterid()<<") - ("<<serverinfo->serverid()<<")"<<std::endl;
        cluster1.push_back(node);
      }
    }

    // Cluster 2
    if(cid==2){
      bool find=false;
      for(int i=0;i<cluster2.size();i++){
        if(cluster2[i].serverID == node.serverID && cluster2[i].isSynchronizer==node.isSynchronizer){
          std::cout<<"ReNew Node"<<"("<<serverinfo->clusterid()<<") - ("<<serverinfo->serverid()<<")"<<std::endl;
          node.type = "alive";
          find=true;
          break;
        }
      }
      if(!find) {
         std::cout<<"New Node"<<"("<<serverinfo->clusterid()<<") - ("<<serverinfo->serverid()<<")"<<std::endl;
          cluster2.push_back(node);
      }
    }

    // Cluster 3
    if(cid==3){
      bool find=false;
      for(int i=0;i<cluster3.size();i++){
        if(cluster3[i].serverID == node.serverID && cluster3[i].isSynchronizer==node.isSynchronizer){
          std::cout<<"ReNew Node"<<"("<<serverinfo->clusterid()<<") - ("<<serverinfo->serverid()<<")"<<std::endl;
          node.type = "alive";
          find=true;
          break;
        }
      }
      if(!find) {
         std::cout<<"New Node"<<"("<<serverinfo->clusterid()<<") - ("<<serverinfo->serverid()<<")"<<std::endl;
        cluster3.push_back(node);
      }
    }
    
    return Status::OK;
  }

  
  Status GetSlave(ServerContext* context, const GetSlaveRequset* getSlaveRequset, ServerInfo* serverinfo) override {
    int id=getSlaveRequset->clusterid();
    if(id==1){
      for(int i=0;i<cluster1.size();i++){
        if(cluster1[i].isMaster==false){
          serverinfo->set_clusterid(id);
          serverinfo->set_serverid(cluster1[i].serverID);
          serverinfo->set_hostname(cluster1[i].hostname);
          serverinfo->set_port(cluster1[i].port);
          serverinfo->set_type(cluster1[i].type);
          break;
        }
      }
    }
    else if(id==2){
       for(int i=0;i<cluster2.size();i++){
        if(cluster2[i].isMaster==false){
          serverinfo->set_clusterid(id);
          serverinfo->set_serverid(cluster2[i].serverID);
          serverinfo->set_hostname(cluster2[i].hostname);
          serverinfo->set_port(cluster2[i].port);
          serverinfo->set_type(cluster2[i].type);
          break;
        }
      }
    }
    else if(id==3){
       for(int i=0;i<cluster3.size();i++){
        if(cluster3[i].isMaster==false){
          serverinfo->set_clusterid(id);
          serverinfo->set_serverid(cluster3[i].serverID);
          serverinfo->set_hostname(cluster3[i].hostname);
          serverinfo->set_port(cluster3[i].port);
          serverinfo->set_type(cluster3[i].type);
          break;
        }
      }
    }
    return Status::OK;
  }


  Status GetAllServers(ServerContext* context,const GetAllServersRequset *getAllServersRequset,GetAllServersResponse* getAllServersResponse) override{
    //std::cout<<"Got GetAllServers"<<std::endl;

    for(int i=0;i<cluster1.size();i++){
      ServerInfo server_info;
      init_serverinfo(&server_info,cluster1[i],1);
      getAllServersResponse->add_serverlist()->CopyFrom(server_info);
    }
      
    for(int i=0;i<cluster2.size();i++){
      ServerInfo server_info;
      init_serverinfo(&server_info,cluster2[i],2);
      getAllServersResponse->add_serverlist()->CopyFrom(server_info);
    }
      
    for(int i=0;i<cluster3.size();i++){
      ServerInfo server_info;
      init_serverinfo(&server_info,cluster3[i],3);
      getAllServersResponse->add_serverlist()->CopyFrom(server_info);
    }
      
   return Status::OK; 
  }

  
  Status Exist(ServerContext* context, const ServerInfo* serverinfo, Confirmation* confirmation) override {
  
    int clusterID=serverinfo->clusterid();
    std::vector<zNode> nodelist;
    if(clusterID==1) nodelist = cluster1;
    if(clusterID==2) nodelist = cluster2;
    if(clusterID==3) nodelist = cluster3;

      for(int i=0;i<nodelist.size();i++){
        zNode node=nodelist[i];
        if(node.serverID == serverinfo->serverid()){
          if(node.isSynchronizer && serverinfo->servertype()=="synchronizer"){
            confirmation->set_status(true);
            return Status::OK;
          }
          if(!node.isSynchronizer && serverinfo->servertype()=="server"){
            confirmation->set_status(true);
            return Status::OK;
          }
        }
      }

      confirmation->set_status(false);
      return Status::OK;
  }


  Status GetSynchronizer(ServerContext* context, const ID* id, ServerInfo* serverinfo) override {
    
    int clusterID=id->id();
    std::vector<zNode> nodelist;
    if(clusterID==1) nodelist = cluster1;
    if(clusterID==2) nodelist = cluster2;
    if(clusterID==3) nodelist = cluster3;

    for(int i=0;i<nodelist.size();i++){
      if(nodelist[i].isSynchronizer){
        init_serverinfo(serverinfo,nodelist[i],clusterID);
        break;
      }
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
      std::vector<zNode*> servers;
      if(cluster1.size()!=0){ 
        for(int i=0;i<cluster1.size();i++){
            servers.push_back(
              &cluster1[i]
            );
        }
      }
      if(cluster2.size()!=0){
         for(int i=0;i<cluster2.size();i++){
            servers.push_back(
              &cluster2[i]
            );
        }
      }
      if(cluster3.size()!=0){
         for(int i=0;i<cluster3.size();i++){
            servers.push_back(
              &cluster3[i]
            );
        }
      }

      for(auto& s : servers){
        if(s->isSynchronizer) continue;
        if(difftime(getTimeNow(),s->last_heartbeat)>10){
          std::cout<<"check "<<s->clusterID<<":"<<s->serverID<<" is down"<<std::endl;
          s->missed_heartbeat = true;
          s->type="down";
          
          if(s->isMaster==true){
            s->isMaster=false;
          }
        }
        else{
          s->missed_heartbeat = false;
          s->type="alive";
        }
      }
      
      sleep(1);
    }
} 


