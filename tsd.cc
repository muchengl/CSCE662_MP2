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
#include <sys/inotify.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <chrono>

#define log(severity, msg) LOG(severity) << msg; google::FlushLogFiles(google::severity); 

#include "sns.grpc.pb.h"
#include "coordinator.grpc.pb.h"
#include "synchronizer.grpc.pb.h"


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

using csce438::GetAllServersRequset;
using csce438::GetAllServersResponse;
using csce438::ID;

using csce438::SynchService;
using csce438::ResynchServerRequest;
using csce438::ResynchServerResponse;

struct Client {
  std::string username;

  bool connected = true;
  // int following_file_size = 0;

  int idx; // used to know user v

  std::vector<Client*> client_followers;
  std::vector<std::string> client_followers_time;

  std::vector<Client*> client_following;
  std::vector<std::string> client_following_time;

  ServerReaderWriter<Message, Message>* stream = 0;

  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

//Vector that stores every client that has been created
std::vector<Client> client_db;
// std::mutex mu;

int getUserVersion(std::string uname){
  for(Client c:client_db){
    if(c.username == uname){
      return c.idx;
    }
  }
  return -1;
}

int lockFile(const std::string& filePath) {
  // std::cout<<"try get lock: "<<filePath<<std::endl;
  //   int fd = open(filePath.c_str(), O_RDWR | O_CREAT,0644);
  //   if (fd == -1) {
  //       return -1;  // 打开文件失败
  //   }

  //   struct flock fl;
  //   fl.l_type = F_WRLCK;   // 设置为写锁
  //   fl.l_whence = SEEK_SET;
  //   fl.l_start = 0;        // 锁定整个文件
  //   fl.l_len = 0;          // 0 表示锁定到文件末尾
  //   fl.l_pid = getpid();

  //   if (fcntl(fd, F_SETLK, &fl) == -1) {
  //       close(fd);
  //       return -1;  // 获取锁失败
  //   }

  //   close(fd);  // 关闭文件描述符
  //   std::cout<<"get lock: "<<filePath<<std::endl;
    return 0;    // 锁定成功
}

int unlockFile(const std::string& filePath) {
    // int fd = open(filePath.c_str(), O_RDWR | O_CREAT,0644);
    // if (fd == -1) {
    //     return -1;  // 打开文件失败
    // }

    // struct flock fl;
    // fl.l_type = F_UNLCK;   // 设置为解锁
    // fl.l_whence = SEEK_SET;
    // fl.l_start = 0;        // 解锁整个文件
    // fl.l_len = 0;          // 0 表示解锁到文件末尾
    // fl.l_pid = getpid();

    // if (fcntl(fd, F_SETLK, &fl) == -1) {
    //     close(fd);
    //     return -1;  // 解锁失败
    // }

    // close(fd);  // 关闭文件描述符
    return 0;    // 解锁成功
}

std::vector<std::string> readFileLines(const std::string& filePath) {
    while(true){
      std::cout<<"TRY GET LOCK\n";
      int k = lockFile(filePath);
      if(k==0) break;
    }
    std::cout<<"GET lock,"<<filePath<<std::endl;

    std::vector<std::string> lines;
    std::string line;
    std::ifstream file(filePath);

    while (getline(file, line)) {
        lines.push_back(line);
    }

    unlockFile(filePath);
    return lines;
}

std::vector<std::string> compareFiles(const std::vector<std::string>& oldLines, const std::vector<std::string>& newLines) {
    std::vector<std::string> addedLines;
    size_t oldSize = oldLines.size();
    size_t newSize = newLines.size();

    int linenum = newSize-oldSize;

    for (int i = linenum-1; i >=0 ; --i) {
        addedLines.push_back(newLines[i]);
    }

    return addedLines;
}


std::vector<std::string> detectFileChanges(
  const std::string& filePath,
  std::vector<std::string>* oldLines
  ) {
    // int inotifyFd = inotify_init();
    // if (inotifyFd < 0) {
    //     std::cerr << "Inotify initialization failed" << std::endl;
    //     return {};
    // }

   //int watchDescriptor = inotify_add_watch(inotifyFd, filePath.c_str(), IN_MODIFY);
    
    //std::vector<std::string> oldLines = readFileLines(filePath);

    std::vector<std::string> addedLines;

    // char buffer[1024];
    // int length = read(inotifyFd, buffer, 1024);

    // for (int i = 0; i < length;) {
    //     struct inotify_event* event = (struct inotify_event*)&buffer[i];
    //     if (event->mask & IN_MODIFY) {
    //         std::cout<<"IN_MODIFY"<<std::endl;
          
            std::vector<std::string> newLines = readFileLines(filePath);
            addedLines = compareFiles(*oldLines, newLines);

            oldLines->clear();
            oldLines->resize(newLines.size());
            std::copy(newLines.begin(), newLines.end(), oldLines->begin());
    //     }
    //     i += sizeof(struct inotify_event) + event->len;
    // }

    // close(inotifyFd);
    return addedLines;
}

void send_msg(std::string line,ServerReaderWriter<Message, Message>* stream,std::string uname){
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

          std::cout<<m.username()<<" send msg," <<uname<<std::endl;

          if(m.username() == uname) return;

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

void monitorFile(std::string filepath,ServerReaderWriter<Message, Message>* stream,std::string uname,int uv){

  std::vector<std::string> init = readFileLines(filepath);
  std::vector<std::string>* oldLines  = &init;

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(4000));
      // check user's version
      if(getUserVersion(uname)!=uv){
        break;
      }


        std::vector<std::string> changes = detectFileChanges(filepath,oldLines);
        if (!changes.empty()) {
            for (const auto& line : changes) {
                std::cout << "New line added: " << line << std::endl;
                send_msg(line,stream,uname);
            }
        }
    }

    std::cout<<"User: "<<uname<<"re-login, "<<"finish monitorFile"<<std::endl;
}

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
  std::string getFollowFileName(){
    std::string timelineFile = "FOLLOWER_"+clusterID+"_"+serverID+".txt";
    return timelineFile;
  }
  std::string getAllUserFileName(){
    std::string file = "USER_"+clusterID+"_"+serverID+".txt";
    return file;
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
    if(reply->type()!="alive"){
      std::cout<<"My slave is DOWN!!! "<<reply->serverid()<<", Host: "<<reply->hostname()<<", Port: "<<reply->port()<<std::endl;
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

  void load_alluser(){
    std::ifstream file(
      getAllUserFileName()
    );
    if (!file.is_open()) {
        std::ofstream file_create(getAllUserFileName(), std::ios::app);
        file_create.close();
        file.open(getAllUserFileName());
    }

    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file: " + getAllUserFileName());
    }

    std::vector<std::string> elements;
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string element;
        if (iss >> element) {
          elements.push_back(element);
          
        }
    }

    for(int i=elements.size()-1;i>=0;i--){
      bool find=false;
      for(Client c: client_db){
        if(c.username == elements[i]){
          find=true;
          break;
        }
      }

      if(find) continue;

      std::cout<<"Can't find user: "<<elements[i]<<", new user"<<std::endl;

      Client *user=new Client();
      user->username = elements[i];
      client_db.push_back(*user);
    }
  }

  void wirte_alluser(){
    std::ofstream file(getAllUserFileName());
    if (!file.is_open()) {
        std::cerr << "Can't open" << std::endl;
        return;
    }

    for(int i=0;i<client_db.size();i++){
      file << client_db[i].username << std::endl;
    }
  }

  void loadFollowerAndFollowingFromFile() {
    // std::unique_lock<std::mutex> lock(mu);
    std::ifstream file(getFollowFileName());

    while(true){
      int k=lockFile(getFollowFileName());
      if(k==0) break;
    }


    std::string userID1, userID2, time;

    // 使用 unordered_map 来快速查找客户端
    std::unordered_map<std::string, Client*> clientMap;
    for (auto& client : client_db) {
        clientMap[client.username] = &client;
        client.client_followers.clear();
        client.client_following.clear();

        client.client_followers_time.clear();
        client.client_following_time.clear();
    }

    while (file >> userID1 >> userID2 >> time) {
        // 确保两个用户都在数据库中
        if (clientMap.find(userID1) != clientMap.end() && clientMap.find(userID2) != clientMap.end()) {
            Client* follower = clientMap[userID1];
            Client* following = clientMap[userID2];

            // 更新关注和被关注的向量
            follower->client_following.push_back(following);
            follower->client_following_time.push_back(time);

            following->client_followers.push_back(follower);
            following->client_followers_time.push_back(time);
        }
    }

    file.close();
    unlockFile(getFollowFileName());
  }

  void writeFollowerAndFollowingToFile() {
    std::ofstream file(getFollowFileName());

    // while(true){
    //   std::cout<<"writeFollowerAndFollowingToFile() Try getting lock: "<<getFollowFileName()<<std::endl;
    //   int k=lockFile(getFollowFileName());
    //   if(k==0) break;
    // }

    // 使用一个 set 来记录已经写入的关系，防止重复
    std::unordered_set<std::string> writtenRelations;

    for (const auto& client : client_db) {
      int idx=0;
        for (const auto* following : client.client_following) {
            std::string relation = client.username + " " + following->username+" "+client.client_following_time[idx];

            // 检查这个关系是否已经被写入
            if (writtenRelations.find(relation) == writtenRelations.end()) {
                file << relation << std::endl;
                writtenRelations.insert(relation);
            }
            idx++;
        }
    }

    file.close();
    // unlockFile(getFollowFileName());
  }

  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    // By Hanzhong Liu
    std::cout<<"LIST"<<std::endl;
    load_alluser();
    loadFollowerAndFollowingFromFile();

    // std::unique_lock<std::mutex> lock(mu);

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

    load_alluser();
    loadFollowerAndFollowingFromFile();

    // std::unique_lock<std::mutex> lock(mu);
    // mu.lock();

    std::cout<<"Follow: "<<request->username()<<" -> "<<request->arguments()[0]<<std::endl;
    if(request->username() == request->arguments()[0]){
      reply->set_msg("Can't follow self");
      return Status::OK; 
    }

    //scarch for user and username2
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

    //successful
    //bulid relationship
    user->client_following.insert(user->client_following.begin(),target);
    target->client_followers.insert(target->client_followers.begin(),user);

    // add time
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds);
    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(seconds.count());
    timestamp->set_nanos(nanos.count());
    std::string t = google::protobuf::util::TimeUtil::ToString(*timestamp);
    
    user->client_following_time.insert(user->client_following_time.begin(),t);
    target->client_followers_time.insert(target->client_followers_time.begin(),t);

    
    // Write relationships back to file
    writeFollowerAndFollowingToFile();

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
    // std::unique_lock<std::mutex> lock(mu);

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
    load_alluser();
    // std::unique_lock<std::mutex> lock(mu);

    std::cout<<"Login:"<<request->username()<<std::endl;

    bool exist=false;
    for(int i=0;i<client_db.size();i++){
      // re-login
      if(client_db[i].username == request->username() && client_db[i].connected==true){
        exist =true;
        client_db[i].stream = nullptr;
        client_db[i].idx++;
      }
      // login back
      else if(client_db[i].username == request->username() && client_db[i].connected==false){
        client_db[i].connected==true;
        client_db[i].stream = nullptr;
        reply->set_msg("OK");
        exist =true;
        client_db[i].idx++;
        //return Status::OK;
      }
    }

    // first time login
    if(!exist){
      Client *newClient = new Client();
      newClient->username = request->username();
      client_db.insert(client_db.begin(),*newClient);
    }
   

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
  
    // save username in file
    // std::string filename = getAllUserFileName();
    // append_file(request->username(),filename);
    wirte_alluser();

    // forward to slave
    login_to_slave(request);

    reply->set_msg("OK");
    return Status::OK;
  }

  void append_file(std::string str,std::string filename){
    while(true){
      int k = lockFile(filename);
      if(k==0) break;
    }
          // open follower's timeline file
          std::ifstream file(filename);

          std::vector<std::string> lines;
          std::string line;
          while (std::getline(file, line)) {
              lines.push_back(line);
          }
          file.close();

          // Insert data from the header
          lines.insert(lines.begin(), str);

          // write lines back
          std::ofstream timeline_file_stream(filename);
          for (const std::string& modified_line : lines) {
              timeline_file_stream << modified_line << std::endl;
          }
          timeline_file_stream.close();
      unlockFile(filename);
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

  std::map<std::string, std::set<std::string>> msgset;

  std::string get_follow_time(std::string follower,std::string following){
    for(Client c:client_db){
      if(c.username != follower) continue;
      for(int i=0;i<c.client_following.size();i++){
        if(c.client_following[i]->username == following){
          return c.client_following_time[i];
        }
      }
    }

    return "";
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

          // avoid resend the same msg
          if(msgset.find(m.username()) == msgset.end()){
            msgset[m.username()] = *(new std::set<std::string>());
          }
          std::set<std::string> mset=msgset[m.username()];
          if(mset.find(line) != mset.end()) continue;
          mset.insert(line);

          // split record into secs
          std::istringstream iss(line);
          std::vector<std::string> words;
          std::string word;
          while (iss >> word) {
            words.push_back(word);
          }

          // built new msg
          Message msg;
          msg.set_username(words[0]);
          msg.set_msg(words[1]);

          if(m.username() == msg.username()) continue;

          // check time
          std::string username = m.username();
          std::string following_name = msg.username();
          std::string follow_time = get_follow_time(username,following_name);
          std::cout<<username<<" follow "<<following_name<<" in "<<follow_time<<"\n";
          if(follow_time > words[2]) {
            std::cout<<"SKIP TIMELINE, "<<line<<" "<<follow_time <<" > "<< words[2]<<std::endl;
            continue;
          }


          google::protobuf::Timestamp *timestamp = new google::protobuf::Timestamp();
          if (google::protobuf::util::TimeUtil::FromString(words[2], timestamp)) {
              std::cout << "Timestamp: " << timestamp->DebugString() << std::endl;
          } else {
              std::cerr << "FAIL!" << std::endl;
          }

          msg.set_allocated_timestamp(timestamp);

          // send msg to user
          stream->Write(msg);          
        } 
  }

  void write_timeline_file(std::string record,std::string name){
    
          /* 
            write file to user's time line
            new msg will be install into the top of the fimeline file
          */
          std::cout<<"WRITE FILE ============="<<"\n";
          
          // open follower's timeline file
          std::string timelineFile = getTimelineFileName(
            name
          );

          while(true){
            int k = lockFile(timelineFile);
            if(k==0) break;
          }


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

          unlockFile(timelineFile);
  }

  void publish_to_follower(std::string record,ServerReaderWriter<Message, Message>* stream,Message m){
    //user has not enter timeline
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
      load_alluser();
      loadFollowerAndFollowingFromFile();

      // std::unique_lock<std::mutex> lock(mu);
      // mu.lock();
      //std::cout<< "TIMELINE INFO: "<<m.username()<<" "<<m.msg()<<std::endl;
      // new login
      if(m.msg() == "join_timeline"){

        std::cout<<"USER: "<<m.username()<<" JOIN TIMELINE!"<<std::endl;
        for(int i=0;i<client_db.size();i++){
           if(client_db[i].username == m.username()){
            client_db[i].stream=stream;
           }
        }

        std::string filename = getTimelineFileName(m.username());
        std::cout<<"Monitor File Change: "<<filename<<std::endl;

        std::string uname = m.username();
        int uv=getUserVersion(uname);

        std::thread wirte([filename,stream,uname,uv]{
          monitorFile(filename,stream,uname,uv);
        });
        wirte.detach();

        /*
          get 20 msgs and send them to user
        */ 
        send_top20msg(stream,m);

        // timeline_to_slave(&m);
       
        // lock.unlock();
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
          // if(client_db[i].client_followers[j]->username != client_db[i].username){ // can't send msg to it self
          //   std::cout<<client_db[i].client_followers[j]->username<<" <<<< msg"<<"\n";
          //   publish_to_follower(record,client_db[i].client_followers[j]->stream,m);
          //   std::cout<<client_db[i].client_followers[j]->username<<" <<<< msg finish"<<"\n";
          // }

          /* 
            write to user's time line
            new msg will be install into the top of the fimeline file
          */
          write_timeline_file(
            record,
            m.username()
          );

          int c1=(stoi(m.username())-1)%3+1;
          int c2=(stoi( client_db[i].client_followers[j]->username) - 1)%3+1;

          if(c1 == c2){
            std::cout<<"Same cluster"<<std::endl;
            write_timeline_file(
              record,
              client_db[i].client_followers[j]->username
            );
          }
        }

        //lient_db[i].client_followers.pop_back();
      }

      timeline_to_slave(&m);
      // lock.unlock();
    }


    return Status::OK;
  }

  Message MakeMessage(const std::string& username, const std::string& msg) {
    Message m;
    m.set_username(username);
    m.set_msg(msg);
    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    m.set_allocated_timestamp(timestamp);
    return m;
  }

  void timeline_to_slave(Message* message){
    std::shared_ptr<SNSService::Stub> slave_stub_ = get_slave_stub();
    if(!slave_stub_) return;

    ClientContext *context = new ClientContext();  
    std::shared_ptr<ClientReaderWriter<Message,Message>> rw = slave_stub_->Timeline(context);
    Message m = MakeMessage(message->username(),message->msg());
    rw->Write(m);
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

  stub->Exist(context,*info,reply);
  if(reply->status()){
    // restart, need resync
    std::cout<<"Restart, begin ReSync"<<std::endl;

    // get syncer's info
    context = new ClientContext(); 
    ID id;
    id.set_id(stoi(clusterID));
    ServerInfo sync;
    stub->GetSynchronizer(context,id,&sync);
    // connect to syncer

    std::string cinfo(sync.hostname() + ":" + sync.port());
    grpc::ChannelArguments args;
    std::shared_ptr<Channel> syncchannel = grpc::CreateCustomChannel(cinfo, grpc::InsecureChannelCredentials(), args);
    std::shared_ptr<SynchService::Stub> stub_sync = SynchService::NewStub(syncchannel);

    // call resync
    context = new ClientContext(); 
    ResynchServerRequest req;
    req.set_serverid(stoi(serverID));

    ResynchServerResponse r;
    stub_sync->ResynchServer(context,req,&r);
  }

  context = new ClientContext(); 
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
