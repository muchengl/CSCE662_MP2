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
#include <algorithm>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "synchronizer.grpc.pb.h"
#include "coordinator.grpc.pb.h"

namespace fs = std::filesystem;

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
// using csce438::ServerList;
using csce438::SynchService;
using grpc::Channel;
// using csce438::AllUsers;
// using csce438::TLFL;

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

using csce438::GetAllUsersRequest;
using csce438::AllUsers;

using csce438::GetFLRequest;
using csce438::GetFLResponse;

using csce438::GetTLRequest;
using csce438::GetTLResponse;

using csce438::ID;

using csce438::ResynchServerRequest;
using csce438::ResynchServerResponse;

int synchID = 1;
std::vector<std::string> get_lines_from_file(std::string);
void run_synchronizer(std::string,std::string,std::string,int);
std::vector<std::string> get_all_users_func(int);
std::vector<std::string> get_tl_or_fl(int, int, bool);

void sync_all_user(std::shared_ptr<CoordService::Stub> coord_stub_,int synchID);
void sync_follow(std::shared_ptr<CoordService::Stub> coord_stub_,int synchID);
void sync_timeline(std::shared_ptr<CoordService::Stub> coord_stub_,int synchID);

std::string getAllUserFileName(std::string clusterID,std::string serverID);
std::string getFollowFileName(std::string clusterID,std::string serverID);
std::string getTimelineFileName(std::string clusterID,std::string serverID,std::string uid);

class SynchServiceImpl final : public SynchService::Service {
  public:
    int SynchID;
    Status GetAllUsers(ServerContext* context, const GetAllUsersRequest* getAllUsersRequest, AllUsers* allusers) override{
        //std::cout<<"Got GetAllUsers"<<std::endl;
        std::vector<std::string> list = get_all_users_func(synchID);

        //package list
        for(auto s:list){
            //std::cout<<"User: "<<s<<std::endl;
            allusers->add_allusers(s);
        }

        //return list
        return Status::OK;
    }

    Status GetFL(ServerContext* context, const GetFLRequest* getFLRequest,GetFLResponse* getFLResponse) override{
        //std::cout<<"Got GetTLFL"<<std::endl;

        std::vector<std::string> fl = get_tl_or_fl(synchID, -1,false);

        //now populate TLFL tl and fl for return
        for(auto s:fl){
            getFLResponse->add_lines(s);
        }

        return Status::OK;
    }

     Status GetTL(ServerContext* context, const GetTLRequest* getTLRequest,GetTLResponse* getTLResponse) override{
        //std::cout<<"Got GetTLFL"<<std::endl;

        std::vector<std::string> fl = get_tl_or_fl(synchID, getTLRequest->uid(),true);

        //now populate TLFL tl and fl for return
        for(auto s:fl){
            getTLResponse->add_lines(s);
        }

        return Status::OK;
    }

    bool copyFile(const std::string& sourceFileName, const std::string& destinationFileName) {
      // 打开源文件
      std::ifstream sourceFile(sourceFileName, std::ios::binary);
      if (!sourceFile.is_open()) {
          std::cerr << "无法打开源文件: " << sourceFileName << std::endl;
          return false;
      }

      // 打开目标文件，ios::trunc标志用于清空文件内容
      std::ofstream destinationFile(destinationFileName, std::ios::binary | std::ios::trunc);
      if (!destinationFile.is_open()) {
          std::cerr << "无法打开目标文件: " << destinationFileName << std::endl;
          sourceFile.close();  // 关闭已打开的源文件
          return false;
      }

      // 复制文件内容
      destinationFile << sourceFile.rdbuf();

      // 关闭文件
      sourceFile.close();
      destinationFile.close();

      return true;
    }

    Status ResynchServer(ServerContext* context, const ResynchServerRequest* resynchServerRequest, ResynchServerResponse* resynchServerResponse){
        //std::cout<<serverinfo->type()<<"("<<serverinfo->serverid()<<") just restarted and needs to be resynched with counterpart"<<std::endl;
        std::string backupServerType;
        
        std::cout<<"******************* ReSync LUNCH! *******************"<<std::endl;

        std::string slaveID = std::to_string(resynchServerRequest->serverid());

        std::string masterID;
        if(slaveID=="1") masterID = "2";
        else masterID="1";

        std::string syncID = std::to_string(SynchID);
         std::string clusterID=syncID;

        std::string MasterUserFilename = getAllUserFileName(clusterID,masterID);
        std::string SlaveUserFilename = getAllUserFileName(clusterID,slaveID);
        std::cout<<"  > Sync User File, "<<MasterUserFilename << " -> " << SlaveUserFilename<<std::endl;
        copyFile(MasterUserFilename,SlaveUserFilename);

        std::string MasterFollowFilename = getFollowFileName(clusterID,masterID);
        std::string SlaveFollowFilename = getFollowFileName(clusterID,slaveID);
        std::cout<<"  > Sync Follow Relationship File, "<<MasterFollowFilename << " -> " << SlaveFollowFilename<<std::endl;
        copyFile(MasterFollowFilename,SlaveFollowFilename);

        std::vector<std::string> allUsers = get_all_users_func(SynchID);
        for(int i=0;i<allUsers.size();i++){
          std::string uid = allUsers[i];

          std::string MasterTlFilename = getTimelineFileName(clusterID,masterID,uid);
          std::string SlaveTlFilename = getTimelineFileName(clusterID,slaveID,uid);
          std::cout<<"  > Sync Timeline File, "<<MasterTlFilename << " -> " << SlaveTlFilename<<std::endl;
          copyFile(MasterTlFilename,SlaveTlFilename);
        }
       


        return Status::OK;
    }
};

void RunServer(std::string coordIP, std::string coordPort, std::string port_no, int synchID){
  //localhost = 127.0.0.1
  std::string server_address("127.0.0.1:"+port_no);
  std::cout<<"Start, "<<server_address<<std::endl;
  SynchServiceImpl service;
  service.SynchID = synchID;

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

  std::thread t1(run_synchronizer,coordIP, coordPort, port_no, synchID);
  /*
  TODO List:
    -Implement service calls
    -Set up initial single heartbeat to coordinator
    -Set up thread to run synchronizer algorithm
  */

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}



int main(int argc, char** argv) {
  
  int opt = 0;
  std::string coordIP="127.0.0.1";
  std::string coordPort="3010";
  std::string port = "3029";

  while ((opt = getopt(argc, argv, "h:j:p:n:")) != -1){
    switch(opt) {
      case 'h':
          coordIP = optarg;
          break;
      case 'j':
          coordPort = optarg;
          break;
      case 'p':
          port = optarg;
          break;
      case 'n':
          synchID = std::stoi(optarg);
          break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }

  RunServer(coordIP, coordPort, port, synchID);
  return 0;
}

void run_synchronizer(std::string coordIP, std::string coordPort, std::string port, int synchID){

    // Stub for coor
    std::string login_info(coordIP + ":" + coordPort);
    grpc::ChannelArguments channel_args;
    std::shared_ptr<Channel> channel = grpc::CreateCustomChannel(login_info, grpc::InsecureChannelCredentials(), channel_args);
    std::shared_ptr<CoordService::Stub> coord_stub_ = CoordService::NewStub(channel);
    std::cout<<"MADE STUB"<<std::endl;

    ServerInfo msg;
    Confirmation c;
    grpc::ClientContext context;

    msg.set_clusterid(synchID);
    msg.set_serverid(synchID);
    msg.set_hostname("127.0.0.1");
    msg.set_port(port);
    msg.set_servertype("synchronizer");
    coord_stub_->Create(&context,msg,&c);

    //send init heartbeat

    //TODO: begin synchronization process
    while(true){
        //change this to 30 eventually
        sleep(5);

        std::cout<<"\n\n******************* SYNC LUNCH! *******************"<<std::endl;

        std::cout<<"========= sync_all_user =============="<<std::endl;
        sync_all_user(coord_stub_,synchID);
          
        std::cout<<"=========== sync_follow ============"<<std::endl;
        sync_follow(coord_stub_,synchID);

        std::cout<<"=========== sync_timeline ============"<<std::endl;
        sync_timeline(coord_stub_,synchID);

        std::cout<<"******************* FINISH! *******************\n"<<std::endl;
	   
    }
    return;
}

std::shared_ptr<SynchService::Stub> get_sync_stub(std::string host,std::string port){
  std::string login_info(host + ":" + port);

  grpc::ChannelArguments channel_args;
  std::shared_ptr<Channel> slave_channel = grpc::CreateCustomChannel(login_info, grpc::InsecureChannelCredentials(), channel_args);
  std::shared_ptr<SynchService::Stub> stub_ = SynchService::NewStub(slave_channel);

  return stub_;
}

std::string getAllUserFileName(std::string clusterID,std::string serverID){
  std::string file = "USER_"+clusterID+"_"+serverID+".txt";
  return file;
}

std::string getFollowFileName(std::string clusterID,std::string serverID){
    std::string file = "FOLLOWER_"+clusterID+"_"+serverID+".txt";
    return file;
}

std::string getTimelineFileName(std::string clusterID,std::string serverID,std::string uid){
    std::string file = "TIMELINE_"+clusterID+"_"+serverID+"_"+uid+".txt";
    return file;
}

void writeSetToFile(const std::set<std::string>& mySet, const std::string& filename) {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);

    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file: " + filename);
    }
    
    for (const auto& element : mySet) {
        file << element << std::endl;
    }

    file.close();
}

void writeListToFile(std::vector<std::string> list, const std::string& filename) {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);

    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file: " + filename);
    }
    
    for (const auto& element : list) {
        file << element << std::endl;
    }

    file.close();
}

/*
  Function for sync all users from other clusters
*/
void sync_all_user(std::shared_ptr<CoordService::Stub> coord_stub_,int synchID){
  
  ClientContext *context = new ClientContext(); 
  GetAllServersRequset *req = new GetAllServersRequset();
  GetAllServersResponse *reply = new GetAllServersResponse();
  coord_stub_->GetAllServers(context,*req,reply);

  std::set<std::string> userSet;

  for (int i = 0; i < reply->serverlist_size(); ++i) {
    const ServerInfo& server_info = reply->serverlist(i);
    //std::cout<<"synchronizer: "<<server_info.hostname()<<":"<<server_info.port()<<std::endl;

    if(server_info.servertype()!="synchronizer") continue; // get user info from synchronizer
    if(server_info.serverid() == synchID) continue; // avoid send req to itself

    std::shared_ptr<SynchService::Stub> stub = get_sync_stub(
      server_info.hostname(),
      server_info.port()
    );

    grpc::ClientContext context;
    GetAllUsersRequest req;
    AllUsers reponse;
    stub->GetAllUsers(&context,req,&reponse);

    for (int j = 0; j < reponse.allusers_size(); ++j) {
      const std::string& user = reponse.allusers(j);
      userSet.insert(user);
      //std::cout<<"Sync user from cluster:"<<server_info.clusterid()<<", User:"<<user<<std::endl;
    }
  }

  // add users from my pair
  std::vector<std::string> list = get_all_users_func(synchID);
  for(auto s:list){
    userSet.insert(s);
  }

  std::cout<<"User List: "<<std::endl;
  for(std::string element : userSet) {
    std::cout << element << std::endl;
  }

  // write users to user file
  std::string master_users_file = getAllUserFileName(std::to_string(synchID),"1");
  std::string slave_users_file = getAllUserFileName(std::to_string(synchID),"2");
  writeSetToFile(
    userSet,master_users_file
  );
  writeSetToFile(
    userSet,slave_users_file
  );
}

void removeDuplicates(std::vector<std::string>& vec) {
    std::sort(vec.begin(), vec.end());

    auto last = std::unique(vec.begin(), vec.end());

    vec.erase(last, vec.end());
}

void aggregateRelations(int synchID,const GetFLResponse& response, std::vector<std::string>& aggregatedRelations) {
    std::vector<std::string> users = get_all_users_func(synchID);
    std::unordered_set<std::string> currentClusterUsers(users.begin(),users.end());

    for (const auto& line : response.lines()) {
        // 每行格式为 "userID1 userID2"
        // userID1 可以是其他集群的关注关系
        std::istringstream iss(line);
        std::string user1, user2;
        if (iss >> user1 >> user2) {
            // 只有当两个用户都在本集群时才加入到聚合关系中
            if (currentClusterUsers.count(user2) > 0) {
                aggregatedRelations.push_back(line);
            }
        }
    }

    removeDuplicates(aggregatedRelations);
}

/*
  Function for sync all follow relationships
*/
void sync_follow(std::shared_ptr<CoordService::Stub> coord_stub_,int synchID){
  ClientContext *context = new ClientContext(); 
  GetAllServersRequset *req = new GetAllServersRequset();
  GetAllServersResponse *reply = new GetAllServersResponse();
  coord_stub_->GetAllServers(context,*req,reply);

  std::string masterf = getFollowFileName(std::to_string(synchID),"1");
  std::string slavef = getFollowFileName(std::to_string(synchID),"2");

  // follow relationships in this cluster
  std::vector<std::string> masterR = get_lines_from_file(
    masterf
  );
  std::vector<std::string> slaveR = get_lines_from_file(
    slavef
  );
  std::vector<std::string> aggregatedRelations;
  if(masterR.size() > slaveR.size()) aggregatedRelations = masterR;
  else aggregatedRelations = slaveR;

  std::cout<<"Old Follow Relationships:"<<std::endl;
  for(int i=0;i<aggregatedRelations.size();i++){
    std::cout<<aggregatedRelations[i]<<std::endl;
  }
 

  for (int i = 0; i < reply->serverlist_size(); ++i) {
    const ServerInfo& server_info = reply->serverlist(i);
    //std::cout<<"synchronizer: "<<server_info.hostname()<<":"<<server_info.port()<<std::endl;

    if(server_info.servertype()!="synchronizer") continue; // get user info from synchronizer
    if(server_info.serverid() == synchID) continue; // avoid send req to itself

    std::shared_ptr<SynchService::Stub> stub = get_sync_stub(
      server_info.hostname(),
      server_info.port()
    );

    // get follow relationship
    grpc::ClientContext context;
    GetFLRequest req;
    GetFLResponse reponse;
    stub->GetFL(&context,req,&reponse);

    aggregateRelations(synchID,reponse,aggregatedRelations);
  }


  std::cout<<"Follow Relationships:"<<std::endl;
  for(int i=0;i<aggregatedRelations.size();i++){
    std::cout<<aggregatedRelations[i]<<std::endl;
  }

  // writeListToFile(aggregatedRelations,getFollowFileName(std::to_string(synchID),"1"));
  writeListToFile(aggregatedRelations,masterf);
  writeListToFile(aggregatedRelations,slavef);
}

std::set<int> getFollowedUsers(int uid,std::string clusterID) {
    std::set<int> followedUsers;

    std::string filename = getFollowFileName(
      clusterID,
      "1"
    );

    std::ifstream file(filename);
    int user1, user2;

    if (!file.is_open()) {
        std::cerr << "无法打开文件！" << std::endl;
        return followedUsers;
    }

    while (file >> user1 >> user2) {
        if (user1 == uid) {
            followedUsers.insert(user2);
        }
    }

    file.close();
    return followedUsers;
}


struct TimelineEntry {
    int id;
    std::string text;
    std::string timestamp;

    bool operator<(const TimelineEntry& other) const {
        if(timestamp == other.timestamp){
          if(text == other.text) return 0;
          return 1;
        }
        return timestamp > other.timestamp;
    }
};

TimelineEntry parseTimelineEntry(const std::string& entryStr) {
    std::istringstream iss(entryStr);
    TimelineEntry entry;
    iss >> entry.id >> entry.text >> entry.timestamp;
    return entry;
}

int lockFile(const std::string& filePath) {
    int fd = open(filePath.c_str(), O_RDWR);
    if (fd == -1) {
        return -1;  // 打开文件失败
    }

    struct flock fl;
    fl.l_type = F_WRLCK;   // 设置为写锁
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;        // 锁定整个文件
    fl.l_len = 0;          // 0 表示锁定到文件末尾
    fl.l_pid = getpid();

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        close(fd);
        return -1;  // 获取锁失败
    }

    close(fd);  // 关闭文件描述符
    return 0;    // 锁定成功
}

int unlockFile(const std::string& filePath) {
    int fd = open(filePath.c_str(), O_RDWR);
    if (fd == -1) {
        return -1;  // 打开文件失败
    }

    struct flock fl;
    fl.l_type = F_UNLCK;   // 设置为解锁
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;        // 解锁整个文件
    fl.l_len = 0;          // 0 表示解锁到文件末尾
    fl.l_pid = getpid();

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        close(fd);
        return -1;  // 解锁失败
    }

    close(fd);  // 关闭文件描述符
    return 0;    // 解锁成功
}

void processAndWriteTimeline(
    const std::vector<std::string>& listEntries,
    int following_userID,
    int synchID,
    int masterID,
    int uid
  ) {

    int slaveID=0;
    if(masterID == 1) {
      slaveID=2;
    }
    else {
      slaveID=1;
    }

    std::set<TimelineEntry> entries;
    std::string fileName = getTimelineFileName(
      std::to_string(synchID),
      std::to_string(masterID),
      std::to_string(uid)
    );
    std::string fileNameSlave = getTimelineFileName(
      std::to_string(synchID),
      std::to_string(slaveID),
      std::to_string(uid)
    );

    // Get lock
    std::cout<<"       >TRY GET LOCK: "<<fileName<<" "<<fileNameSlave<<std::endl;
    while(true){
      int k=lockFile(fileName);
      if(k==0) break;
    }
    while(true){
      int k=lockFile(fileNameSlave);
      if(k==0) break;
    }
    std::cout<<"       >GET LOCK: "<<fileName<<" "<<fileNameSlave<<std::endl;
    
    std::ifstream fileIn(fileName);
    TimelineEntry entry;

    // 从文件读取数据
    if (fileIn.is_open()) {
        std::string line;
        while (std::getline(fileIn, line)) {
            entries.insert(parseTimelineEntry(line));
        }
        fileIn.close();
    } else {
        std::cerr << "无法打开文件进行读取！" << std::endl;
    }

    // 添加列表中的数据
    for (const auto& listEntryStr : listEntries) {
      // If this entry is not published by following, should not add it
        TimelineEntry entry = parseTimelineEntry(listEntryStr);
        if(entry.id != following_userID) continue;

        entries.insert(parseTimelineEntry(listEntryStr));
    }

    // 写入数据到文件
    std::cout<<"       >"<<"FLIENAME 01: "<<" : "<<fileName<<std::endl;
    std::ofstream fileOut(fileName);
    if (fileOut.is_open()) {
        for (const auto& e : entries) {
            std::cout<<"       >"<<"W: "<<" : "<<e.id << " " << e.text << " " << e.timestamp<<std::endl;
            fileOut << e.id << " " << e.text << " " << e.timestamp << std::endl;
        }
        fileOut.close();
    } else {
        std::cerr << "Can't write file" << std::endl;
    }
    



    std::cout<<"       >"<<"FLIENAME 02: "<<" : "<<fileNameSlave<<std::endl;
    std::ofstream fileOutSlave(fileNameSlave);
    if (fileOutSlave.is_open()) {
        for (const auto& e : entries) {
            std::cout<<"       >"<<"W: "<<" : "<<e.id << " " << e.text << " " << e.timestamp<<std::endl;
            fileOutSlave << e.id << " " << e.text << " " << e.timestamp << std::endl;
        }
        fileOutSlave.close();
    } else {
        std::cerr << "Can't write file" << std::endl;
    }

    unlockFile(fileName);
    unlockFile(fileNameSlave);
}

/*
  Function for sync timeline
*/
void sync_timeline(std::shared_ptr<CoordService::Stub> coord_stub_,int synchID){
  ClientContext *context = new ClientContext(); 
  GetAllServersRequset *req = new GetAllServersRequset();
  GetAllServersResponse *reply = new GetAllServersResponse();
  coord_stub_->GetAllServers(context,*req,reply);

  context = new ClientContext(); 
  ID *id = new ID();
  id->set_id(synchID);
  ServerInfo *master = new ServerInfo();
  coord_stub_->GetServer(context,*id,master);
  
  int masterID = master->serverid();
  std::cout<<"Master is: "<<masterID<<std::endl;

  // 1. t all user
  std::vector<std::string> allUser = get_all_users_func(synchID);

  // 遍历user，如果当前user属于本集群，（userid-1）%3 + 1 ==syncID
  for(int i=0;i<allUser.size();i++){
    std::string user=allUser[i];
    int uid = stoi(user);
    int cid = (uid-1)%3 + 1;

    if(cid != synchID) continue;
    std::cout<<"Sync timeline for user: "<<uid<<std::endl;

    std::set<int> followedUsers = getFollowedUsers(
      uid,
      std::to_string(synchID)
    );
    for (int following : followedUsers) {
      ServerInfo info;
      int targetClusterId = (following-1)%3+1;
      // no need to sync in same cluster
      // if(targetClusterId == synchID) continue;

      std::cout<<"  > Sync timeline from user: "<<targetClusterId<<std::endl;

      // find sync
      for (int j = 0; j < reply->serverlist_size(); ++j) {
        const ServerInfo& server_info = reply->serverlist(j);
        if(server_info.servertype()!="synchronizer") continue; // get user info from synchronizer
        if(server_info.clusterid() == targetClusterId){
          info = server_info;
          break;
        }
      }

      std::cout<<"    > build stub with: "<<targetClusterId<<"  "<<info.hostname()+":"+info.port()<<std::endl;
      std::shared_ptr<SynchService::Stub> stub = get_sync_stub(
        info.hostname(),
        info.port()
      );

      // get TIMELINE
      grpc::ClientContext context;
      GetTLRequest req;
      req.set_uid(following);

      GetTLResponse reponse;
      stub->GetTL(&context,req,&reponse);

      std::cout<<"    > get timeline from: "<<following<<std::endl;
      std::vector<std::string> listEntries;
      for (int j = 0; j < reponse.lines_size(); ++j) {
        std::string line = reponse.lines(j);
        std::cout<<"       >"<<j<<" : "<<line<<std::endl;
        listEntries.push_back(line);
      }
      std::cout<<"    > "<<"processAndWriteTimeline"<<std::endl;
      processAndWriteTimeline(listEntries,following,synchID,masterID,uid);
    }
  }
}


std::vector<std::string> get_lines_from_file(std::string filename){
  std::vector<std::string> users;
  std::string user;
  std::ifstream file; 
  file.open(filename);
  if(file.peek() == std::ifstream::traits_type::eof()){
    //return empty vector if empty file
    //std::cout<<"returned empty vector bc empty file"<<std::endl;
    file.close();
    return users;
  }
  while(file){
    getline(file,user);

    if(!user.empty())
      users.push_back(user);
  } 

  file.close();

  //std::cout<<"File: "<<filename<<" has users:"<<std::endl;
  /*for(int i = 0; i<users.size(); i++){
    std::cout<<users[i]<<std::endl;
  }*/ 

  return users;
}

bool file_contains_user(std::string filename, std::string user){
    std::vector<std::string> users;
    //check username is valid
    users = get_lines_from_file(filename);
    for(int i = 0; i<users.size(); i++){
      //std::cout<<"Checking if "<<user<<" = "<<users[i]<<std::endl;
      if(user == users[i]){
        //std::cout<<"found"<<std::endl;
        return true;
      }
    }
    //std::cout<<"not found"<<std::endl;
    return false;
}

std::vector<std::string> get_all_users_func(int synchID){
    //read all_users file master and client for correct serverID
    std::string master_users_file = getAllUserFileName(std::to_string(synchID),"1");
    std::string slave_users_file = getAllUserFileName(std::to_string(synchID),"2");

    //take longest list and package into AllUsers message
    std::vector<std::string> master_user_list = get_lines_from_file(master_users_file);
    std::vector<std::string> slave_user_list = get_lines_from_file(slave_users_file);

    if(master_user_list.size() >= slave_user_list.size())
        return master_user_list;
    else
        return slave_user_list;
}

std::vector<std::string> get_tl_or_fl(int synchID, int clientID,bool tl){
    std::string master_fn = "";
    std::string slave_fn = "";
    
    if(tl){
        master_fn = getTimelineFileName(std::to_string(synchID),"1",std::to_string(clientID));
        slave_fn = getTimelineFileName(std::to_string(synchID),"2",std::to_string(clientID));
    }else{
        master_fn = getFollowFileName(std::to_string(synchID),"1");
        slave_fn = getFollowFileName(std::to_string(synchID),"2");
    }

    std::vector<std::string> m = get_lines_from_file(master_fn);
    std::vector<std::string> s = get_lines_from_file(slave_fn);

    if(m.size()>=s.size()){
        return m;
    }else{
        return s;
    }
}