/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include<glog/logging.h>
#define log(severity, msg) LOG(severity) << msg; google::FlushLogFiles(google::severity); 

#include "sns.grpc.pb.h"
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
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;

using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::ClientContext;

using csce438::CoordService;
using grpc::Channel;
using grpc::ClientContext;
using csce438::Confirmation;
using csce438::ServerInfo;
using csce438::GetSlaveRequset;


struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
  void loadFollowerAndFollowingFromFile();
  void writeFollowerAndFollowingtoFile();
};

void Client::loadFollowerAndFollowingFromFile(){
  
}

void Client::writeFollowerAndFollowingtoFile(){

}

//Vector that stores every client that has been created
std::vector<Client> client_db;
std::mutex mu;

class SNSServiceImpl final : public SNSService::Service {
public:
  std::string clusterID;
  std::string serverID;
  std::string coordinatorIP;
  std::string coordinatorPort;

  std::shared_ptr<CoordService::Stub> stub_;
  
  std::string getTimelineFileName(std::string username){
    std::string timelineFile = "TIMELINE_"+clusterID+"_"+serverID+"_"+username+".txt";
    return timelineFile;
  }
  std::string getFollowerFileName(std::string username){
    std::string timelineFile = "FOLLOWER_"+clusterID+"_"+serverID+"_"+username+".txt";
    return timelineFile;
  }
  std::string getFollowingFileName(std::string username){
    std::string timelineFile = "FOLLOWING_"+clusterID+"_"+serverID+"_"+username+".txt";
    return timelineFile;
  }

  std::shared_ptr<SNSService::Stub>  get_slave_stub(){
    // 1. get slave info
    ClientContext *context = new ClientContext(); 

    GetSlaveRequset *req = new GetSlaveRequset();
    req->set_clusterid(atoi(clusterID.c_str()));

    ServerInfo *reply = new ServerInfo();
    stub_->GetSlave(context,*req,reply);
    if(std::to_string(reply->serverid()) == serverID) {
      std::cout<<"I am slave"<<std::endl;
      return std::shared_ptr<SNSService::Stub>();
    }

    std::cout<<"My slave is: "<<reply->serverid()<<", Host: "<<reply->hostname()<<", Port: "<<reply->port()<<std::endl;
    std::string host = reply->hostname();
    std::string port = reply->port();

    // 2. connect to slave
    std::string login_info(host + ":" + port);

    grpc::ChannelArguments channel_args;
    std::shared_ptr<Channel> slave_channel = grpc::CreateCustomChannel(login_info, grpc::InsecureChannelCredentials(), channel_args);
    std::shared_ptr<SNSService::Stub> slave_stub_ = SNSService::NewStub(slave_channel);

    return slave_stub_;
  }

  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    // By Hanzhong Liu
    std::unique_lock<std::mutex> lock(mu);

    // add all username from client_db to list_reply
    for(int i=0;i<client_db.size();i++){
      list_reply->add_all_users(client_db[i].username);
    }

    // add all user's followers from client_db to list_reply
     for(int i=0;i<client_db.size();i++){
      if(client_db[i].username != request->username()) continue;
      for(int j=0;j<client_db[i].client_followers.size();j++){
        list_reply->add_followers(client_db[i].client_followers[j]->username);
      }
    }

    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // By Hanzhong Liu
    std::unique_lock<std::mutex> lock(mu);

    std::cout<<"Follow: "<<request->username()<<" -> "<<request->arguments()[0]<<std::endl;
    if(request->username() == request->arguments()[0]){
      reply->set_msg("Can't follow self");
      return Status::OK; 
    }

    // scarch for user and username2
    Client *user,*target;
    for(int i=0;i<client_db.size();i++){
      if(client_db[i].username == request->username()){
        user = &client_db[i];
      }
    }
    bool flag=true;
    for(int i=0;i<client_db.size();i++){
      if(client_db[i].username == request->arguments()[0]){
        target = &client_db[i];
        flag=false;
        break;
      }
    }

    // can't find user
    if(flag){
      std::cout<<"Can't find "<<request->arguments()[0]<<std::endl;
      reply->set_msg("NO_TARGET");
      return Status::OK;
    }

    // re-follow
    for(int i=0;i<user->client_following.size();i++){
      if(user->client_following[i]->username == request->arguments()[0]){
        reply->set_msg("RE-FOLLOW");
        return Status::OK;
      }
    }

    // successful
    // bulid relationship
    user->client_following.insert(user->client_following.begin(),target);
    target->client_followers.insert(target->client_followers.begin(),user);

    // forward to slave
    follow_to_slave(request);

    reply->set_msg("OK");
    return Status::OK; 
  }

  void follow_to_slave(const Request* request){
    std::shared_ptr<SNSService::Stub> slave_stub_ = get_slave_stub();
    if(!slave_stub_) return;

    // 3. send req
    Reply *r=new Reply();
    ClientContext *context = new ClientContext(); 
    slave_stub_->Follow(context,*request,r);
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // By Hanzhong Liu
    std::unique_lock<std::mutex> lock(mu);

    std::cout<<"UnFollow:"<<request->username()<<" -> "<<request->arguments()[0]<<std::endl;

    Client *user,*target;

    // search for user and username2
    for(int i=0;i<client_db.size();i++){
      if(client_db[i].username == request->username()){
        user = &client_db[i];
      }
    }
    bool flag=true;
    for(int i=0;i<client_db.size();i++){
      if(client_db[i].username == request->arguments()[0]){
        target = &client_db[i];
        flag=false;
        break;
      }
    }

    // user is not follow target-user
    if(flag){
      std::cout<<"Can't find "<<request->arguments()[0]<<std::endl;
      reply->set_msg("NO_TARGET");
      return Status::OK;
    }

    /*
      remove relationship
      */
    int idx=0;
    flag=true;
    for (Client* u: user->client_following) {
        if(u->username == request->arguments()[0]){
          user->client_following.erase(user->client_following.begin()+idx);
          flag=false;
          break;
        }
        idx++;
    }

    idx=0;
    for (Client* u: target->client_followers) {
        if(u->username == request->username()){
          target->client_followers.erase(target->client_followers.begin()+idx);
          break;
        }
        idx++;
    }

    unfollow_to_slave(request);

    reply->set_msg("OK");
    return Status::OK; 
  }

  void unfollow_to_slave(const Request* request){
    std::shared_ptr<SNSService::Stub> slave_stub_ = get_slave_stub();
    if(!slave_stub_) return;

    // 3. send req
    Reply *r=new Reply();
    ClientContext *context = new ClientContext(); 
    slave_stub_->UnFollow(context,*request,r);
  }

  // RPC Login
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // By Hanzhong Liu
    std::unique_lock<std::mutex> lock(mu);

    std::cout<<"Login:"<<request->username()<<std::endl;

    for(int i=0;i<client_db.size();i++){
      // re-login
      if(client_db[i].username == request->username() && client_db[i].connected==true){
        reply->set_msg("Deny");
        return Status::OK;
      }
      // login back
      if(client_db[i].username == request->username() && client_db[i].connected==false){
        client_db[i].connected==true;
        reply->set_msg("OK");
        return Status::OK;
      }
    }

    // first time login
    Client *newClient = new Client();
    newClient->username = request->username();

    client_db.insert(client_db.begin(),*newClient);

    // create a timeline file
    std::string timelineFile = getTimelineFileName(request->username());
    if (!fileExists(timelineFile)) {
      std::cout<<"Create file"<<std::endl;
      std::ofstream timeline_file(timelineFile);
      if (!timeline_file.is_open()) {
          std::cerr << "fail" << std::endl;
          reply->set_msg("Deny");
          return Status::OK;
      }
      timeline_file.close();
    }
  
    // forward to slave
    login_to_slave(request);


    reply->set_msg("OK");
    return Status::OK;
  }

  void login_to_slave(const Request* request){
    std::shared_ptr<SNSService::Stub> slave_stub_ = get_slave_stub();
    if(!slave_stub_) return;

    // 3. send req
    Reply *r=new Reply();
    ClientContext *context = new ClientContext(); 
    slave_stub_->Login(context,*request,r);
  }

  bool fileExists(const std::string& filename) {
      std::ifstream file(filename);
      return file.good();
  }


  void send_top20msg(ServerReaderWriter<Message, Message>* stream,Message m){
        /*
          get 20 msgs and send them to user
        */ 
        std::string filename = getTimelineFileName(m.username());
        std::ifstream timeline_file(filename);
        
        int idx=0;
        std::string line;
        while (idx<20 && std::getline(timeline_file, line)) {
          idx++;
          std::cout<<"INFO: "<<line<<"\n";

          // split record into secs
          std::istringstream iss(line);
          std::vector<std::string> words;
          std::string word;
          while (iss >> word) {
            words.push_back(word);
          }

          // built new msg
          Message m;
          m.set_username(words[0]);
          m.set_msg(words[1]);

          google::protobuf::Timestamp *timestamp = new google::protobuf::Timestamp();
          if (google::protobuf::util::TimeUtil::FromString(words[2], timestamp)) {
              std::cout << "Timestamp: " << timestamp->DebugString() << std::endl;
          } else {
              std::cerr << "FAIL!" << std::endl;
          }

          m.set_allocated_timestamp(timestamp);

          // send msg to user
          stream->Write(m);          
        } 
  }

  void write_timeline_file(std::string record,std::string follower_name){
          /* 
            write file to user's time line
            new msg will be install into the top of the fimeline file
          */
          std::cout<<"WRITE FILE ============="<<"\n";
          
          // open follower's timeline file
          std::string timelineFile = getTimelineFileName(
            follower_name
          );
          std::ifstream timelinefile(timelineFile);

          std::vector<std::string> lines;
          std::string line;
          while (std::getline(timelinefile, line)) {
              lines.push_back(line);
          }
          timelinefile.close();

          // Insert data from the header
          lines.insert(lines.begin(), record);

          // write lines back
          std::ofstream timeline_file_stream(timelineFile);
          for (const std::string& modified_line : lines) {
              timeline_file_stream << modified_line << std::endl;
          }
          timeline_file_stream.close();
  }

  void publish_to_follower(std::string record,ServerReaderWriter<Message, Message>* stream,Message m){
    // user has not enter timeline
    if(stream==nullptr){
      std::cout<<"NULL! can't send to follower"<<"\n";
      return;
    }

    // user already enter timeline, send to it
    stream->Write(m);
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // By Hanzhong Liu
    Message m;
    while(stream->Read(&m)){
      std::unique_lock<std::mutex> lock(mu);

      // new login
      if(m.msg() == "join_timeline"){

        std::cout<<"USER: "<<m.username()<<" JOIN TIMELINE!"<<std::endl;
        for(int i=0;i<client_db.size();i++){
           if(client_db[i].username == m.username()){
            client_db[i].stream=stream;
           }
        }

        /*
          get 20 msgs and send them to user
        */ 
        send_top20msg(stream,m);
       
        lock.unlock();
        continue;
      }

      /* 
        publish msg to all followers' timeline
      */
      
      std::string time = google::protobuf::util::TimeUtil::ToString(m.timestamp());
      std::string record=m.username()+" "+m.msg()+" "+time;

      for(int i=0;i<client_db.size();i++){
        if(client_db[i].username != m.username()){
          continue;
        }

        //client_db[i].client_followers.push_back(&client_db[i]);

        for(int j=0;j<client_db[i].client_followers.size();j++){
          
          // publish msg to follower
          if(client_db[i].client_followers[j]->username != client_db[i].username){ // can't send msg to it self
            std::cout<<client_db[i].client_followers[j]->username<<" <<<< msg"<<"\n";

            publish_to_follower(record,client_db[i].client_followers[j]->stream,m);

            std::cout<<client_db[i].client_followers[j]->username<<" <<<< msg finish"<<"\n";
          }

          /* 
            write to user's time line
            new msg will be install into the top of the fimeline file
          */
          write_timeline_file(
            record,
            client_db[i].client_followers[j]->username
          );
        }

        //lient_db[i].client_followers.pop_back();
      }

      timeline_to_slave(m);
      lock.unlock();
    }


    return Status::OK;
  }

  void timeline_to_slave(Message message){
    std::shared_ptr<SNSService::Stub> slave_stub_ = get_slave_stub();
    if(!slave_stub_) return;

    // 3. send req
    ClientContext *context = new ClientContext();  
    std::shared_ptr<ClientReaderWriter<Message,Message>> rw = slave_stub_->Timeline(context);
    rw->Write(message);
  }

};



void Heartbeat(std::shared_ptr<CoordService::Stub> stub_,std::string *serverID,std::string *clusterID){
  while (true)
  {
    
    ClientContext *context = new ClientContext();  
    ServerInfo *info = new ServerInfo();
    info->set_serverid(atoi(serverID->c_str()));
    info->set_clusterid(atoi(clusterID->c_str()));

    Confirmation *reply=new Confirmation();

    stub_->Heartbeat(context,*info,reply);
    
    //std::cout<<"Send Heartbeat MSG:"<<atoi(serverID->c_str())<<std::endl;

    sleep(1);
  }
  
}

void RunServer(
    std::string port_no,
    std::string clusterID,
    std::string serverID,
    std::string coordinatorIP,
    std::string coordinatorPort
  ) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  service.clusterID=clusterID;
  service.serverID=serverID;
  service.coordinatorIP=coordinatorIP;
  service.coordinatorPort=coordinatorPort;

  ServerInfo *info = new ServerInfo();
  int id=atoi(clusterID.c_str());
  // std::cout<<"clusterID: "<<id<<std::endl;
  info->set_clusterid(id);

  id=atoi(serverID.c_str());
  // std::cout<<"serverID: "<<id<<std::endl;
  info->set_serverid(id);

  info->set_hostname("127.0.0.1");
  info->set_port(port_no);

  // work as a server, not Synchronizer
  info->set_servertype("server");


  // connect to coordinator service
  std::string login_info(coordinatorIP + ":" + coordinatorPort);
  grpc::ChannelArguments channel_args;
  std::shared_ptr<Channel> channel = grpc::CreateCustomChannel(login_info, grpc::InsecureChannelCredentials(), channel_args);
  service.stub_ = CoordService::NewStub(channel);

  std::shared_ptr<CoordService::Stub> stub = service.stub_;

  ClientContext *context = new ClientContext();  
  Confirmation *reply=new Confirmation();
  stub->Create(context,*info,reply);

  std::thread hb([stub,serverID,clusterID]{
      std::string cid=clusterID;
      std::string sid=serverID;
      Heartbeat(stub,&sid,&cid);
  });
	hb.detach();



  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  log(INFO, "Server listening on "+server_address);

  server->Wait();
}

int main(int argc, char** argv) {

  std::string port = "3010";

  std::string clusterID="1";
  std::string serverID="1";
  std::string coordinatorIP="127.0.0.1";
  std::string coordinatorPort="3010";
  
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:s:h:k:p:")) != -1){
    switch(opt) {
      case 'c':
          clusterID=optarg;break;
      case 's':
          serverID=optarg;break;
      case 'h':
          coordinatorIP=optarg;break;   
      case 'k':
          coordinatorPort=optarg;break;  
      case 'p':
          port = optarg;break;    
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
  
  std::string log_file_name = std::string("server-") + port;
  google::InitGoogleLogging(log_file_name.c_str());
  log(INFO, "Logging Initialized. Server starting...");
  std::cout<<"clusterID:"<<clusterID<<" serverID:"<<serverID<<std::endl;
  RunServer(port,clusterID,serverID,coordinatorIP,coordinatorPort);

  return 0;
}
