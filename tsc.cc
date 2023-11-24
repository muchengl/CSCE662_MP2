#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <unistd.h>
#include <csignal>
#include <grpc++/grpc++.h>
#include "client.h"
#include "google/protobuf/util/time_util.h"
#include <chrono>

#include "sns.grpc.pb.h"
#include "coordinator.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;


using csce438::CoordService;
using grpc::Channel;
using grpc::ClientContext;
using csce438::Confirmation;
using csce438::ServerInfo;
using csce438::ID;


void sig_ignore(int sig) {
  std::cout << "Signal caught " + sig<<"\n";
  exit(0);
}

Message MakeMessage(const std::string& username, const std::string& msg) {
    Message m;
    m.set_username(username);
    m.set_msg(msg);

    // google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    // timestamp->set_seconds(time(NULL));
    // timestamp->set_nanos(0);

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds);
    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(seconds.count());
    timestamp->set_nanos(nanos.count());

    m.set_allocated_timestamp(timestamp);
    return m;
}


class Client : public IClient
{
public:
  Client(const std::string& hname,
	 const std::string& uname,
	 const std::string& p)
    :hostname(hname), username(uname), port(p) {}

  
protected:
  virtual int connectTo();
  virtual int connectToServer();
  virtual IReply processCommand(std::string& input);
  virtual void processTimeline();
  virtual int updateServerType();

private:
  std::string hostname;
  std::string username;
  std::string port;

  std::string server_hostname;
  std::string server_port;
  std::string server_type;
  
  // You can have an instance of the client stub
  // as a member variable.
  std::unique_ptr<SNSService::Stub> stub_;
  std::shared_ptr<Channel> channel;

  std::shared_ptr<CoordService::Stub> coord_stub_;
  
  IReply Login();
  IReply List();
  IReply Follow(const std::string &username);
  IReply UnFollow(const std::string &username);
  void   Timeline(const std::string &username);
};


///////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////
int Client::connectTo()
{
  // ------------------------------------------------------------
  // In this function, you are supposed to create a stub so that
  // you call service methods in the processCommand/porcessTimeline
  // functions. That is, the stub should be accessible when you want
  // to call any service methods in those functions.
  // Please refer to gRpc tutorial how to create a stub.
  // ------------------------------------------------------------

    std::string login_info(hostname + ":" + port);
    grpc::ChannelArguments channel_args;
    std::shared_ptr<Channel> channel = grpc::CreateCustomChannel(login_info, grpc::InsecureChannelCredentials(), channel_args);
    std::shared_ptr<CoordService::Stub> stub = CoordService::NewStub(channel);
    coord_stub_ = stub;

    ClientContext *context = new ClientContext();  
    ID *id=new ID();
    id->set_id(atoi(username.c_str()));
    ServerInfo *reply = new ServerInfo();

    stub->GetServer(context,*id,reply);

    server_hostname = reply->hostname();
    server_port = reply->port();
    server_type = reply->type();
    //std::cout<<"Client: "<<username<<" ,cluster ID: "<<reply->clusterid()<<" ,server ID: "<<reply->serverid()<<" ,server Type: "<<server_type<<std::endl;

    if(server_type!="alive"){
      return -1;
    }

    return connectToServer();
}

int Client::connectToServer(){
    std::string login_info(server_hostname + ":" + server_port);
    //std::cout<<login_info<<std::endl;

    grpc::ChannelArguments channel_args;
    channel = grpc::CreateCustomChannel(login_info, grpc::InsecureChannelCredentials(), channel_args);

    stub_ = SNSService::NewStub(channel);

    IReply r = Login();

    if(!r.grpc_status.ok()) {
      return -1;
    }

    if(!r.comm_status==IStatus::SUCCESS){
      //std::cout<<"re-login!"<<std::endl;
      return -1;
    }
    return 1;
    //std::cout<<login_info<<std::endl;
}

int Client::updateServerType(){
    ClientContext *context = new ClientContext();  
    ID *id=new ID();
    id->set_id(atoi(username.c_str()));
    ServerInfo *reply = new ServerInfo();

    coord_stub_->GetServer(context,*id,reply);

    server_hostname = reply->hostname();
    server_port = reply->port();
    server_type = reply->type();
   //std::cout<<"Client: "<<username<<" ,cluster ID: "<<reply->clusterid()<<" ,server ID: "<<reply->serverid()<<" ,server Type: "<<server_type<<std::endl;

    if(server_type!="alive"){
      return -1;
    }
}

IReply Client::processCommand(std::string& input)
{
    IReply ire;
    
    // By Hanzhong Liu
    // 1.Split Input by Space
    std::istringstream iss(input);	
    std::vector<std::string> tokens;

    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    // 2.Check server status
    if(server_type!="alive"){
      updateServerType();
      if(server_type=="alive") {
        Login();
      }
    }

    updateServerType();
    if(server_type!="alive"){
      ire.comm_status = IStatus::FAILURE;
      return ire;
    }

    grpc_connectivity_state state = channel->GetState(true);
    if(state == 0 ||state == GRPC_CHANNEL_TRANSIENT_FAILURE || state==GRPC_CHANNEL_SHUTDOWN){
      server_type="down";
      ire.comm_status = IStatus::FAILURE;
      return ire;
    }

    // 3.Match
    if(tokens[0]=="FOLLOW"){
      ire = Follow(tokens[1]);
    }
    else if(tokens[0]=="UNFOLLOW"){
      ire = UnFollow(tokens[1]);
    }
    else if(tokens[0]=="LIST"){
      ire = List();
    }
    else if(tokens[0]=="TIMELINE"){
      ire.grpc_status = Status::OK;
      ire.comm_status = IStatus::SUCCESS;
    }
    else{}

    return ire;
}


void Client::processTimeline()
{
    Timeline(username);
}

// List Command
IReply Client::List() {
    // By Hanzhong Liu
    IReply ire;
   
    ClientContext *context = new ClientContext();  

    Request *request = new Request();
    request->set_username(username);

    ListReply *reply = new ListReply();

    Status status = stub_->List(context,*request, reply);
    if(!status.ok()){
      ire.comm_status = IStatus::FAILURE;
      return ire;
    }
    
    for(int i=0;i<reply->all_users_size();i++){
      ire.all_users.insert(ire.all_users.begin(),reply->all_users()[i]);
    }

    for(int i=0;i<reply->followers_size();i++){
      ire.followers.insert(ire.followers.begin(),reply->followers()[i]);
    }

    ire.comm_status = IStatus::SUCCESS;

    return ire;
}

// Follow Command        
IReply Client::Follow(const std::string& username2) {
    // By Hanzhong Liu
    IReply ire; 
      
    ClientContext *context = new ClientContext();  

    Request *request = new Request();
    request->set_username(username);
    request->add_arguments(username2);

    Reply *reply = new Reply();

    Status status = stub_->Follow(context,*request, reply);

    ire.grpc_status = Status::OK;
    if(reply->msg()=="OK"){
      ire.comm_status = IStatus::SUCCESS;
    }
    else if(reply->msg() == "NO_TARGET"){
      ire.comm_status = IStatus::FAILURE_INVALID_USERNAME;
    }
    else if(reply->msg() == "Can't follow self"){
      ire.comm_status = IStatus::FAILURE_ALREADY_EXISTS;
    }
    else if(reply->msg() == "RE-FOLLOW"){
       ire.comm_status = IStatus::FAILURE_ALREADY_EXISTS;
    }
    else{
      ire.comm_status = IStatus::FAILURE_UNKNOWN;
    }

    return ire;
}

// UNFollow Command  
IReply Client::UnFollow(const std::string& username2) {

    // By Hanzhong Liu
    IReply ire; 
      
    ClientContext *context = new ClientContext();  

    Request *request = new Request();
    request->set_username(username);
    request->add_arguments(username2);

    Reply *reply = new Reply();

    Status status = stub_->UnFollow(context,*request, reply);

    ire.grpc_status = Status::OK;
    if(reply->msg()=="OK"){
      ire.comm_status = IStatus::SUCCESS;
    }
    else if(reply->msg() == "NO_TARGET"){
      ire.comm_status = IStatus::FAILURE_INVALID_USERNAME;
    }
    else{
      ire.comm_status = IStatus::FAILURE_UNKNOWN;
    }

    return ire;
}

// Login Command  
IReply Client::Login() {
    // By Hanzhong Liu
    IReply ire;
  
    ClientContext *context = new ClientContext();  

    Request *request = new Request();
    request->set_username(username);

    Reply *reply = new Reply();

    Status status = stub_->Login(context,*request, reply);

    ire.grpc_status = status;
    if(reply->msg()=="OK"){
      ire.comm_status = IStatus::SUCCESS;
    }
    else{
      ire.comm_status = IStatus::FAILURE_ALREADY_EXISTS;
    }
   
    return ire;
}

void TimelineWrite(std::shared_ptr<grpc::ClientReaderWriter<Message,Message>> rw,std::string *username){
  
  while(true){
    std::string input = getPostMessage();;

    input = input.substr(0,input.length()-1);
   
    Message message = MakeMessage(*username,input);
    rw->Write(message);
  }
}

void TimelineRead(std::shared_ptr<grpc::ClientReaderWriter<Message,Message>> rw){
  Message *message = new Message();
  while(rw->Read(message)){
    // print info
    std::time_t timeValue = static_cast<time_t>(message->timestamp().seconds());
    displayPostMessage(message->username(),message->msg(),timeValue);
    // std::cout<<"\n";
  }
}

// Timeline Command
void Client::Timeline(const std::string& username) {

    // ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions 
    // in client.cc file for both getting and displaying messages 
    // in timeline mode.
    // ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
    // ------------------------------------------------------------
  
    // By Hanzhong Liu
    ClientContext *context = new ClientContext();  
    std::shared_ptr<ClientReaderWriter<Message,Message>> rw = stub_->Timeline(context);
    
    Message message = MakeMessage(username,"join_timeline");
    rw->Write(message);

    std::string *s=new std::string(username);

    std::thread wirte([rw,s]{
      TimelineWrite(rw,s);
    });
	  wirte.detach();

    std::thread read([rw]{
      TimelineRead(rw);
    });
	  read.detach();

    // loop forever
    while(true){}
    
}




//////////////////////////////////////////////
// Main Function
/////////////////////////////////////////////
int main(int argc, char** argv) {

  std::string hostname = "127.0.0.1";
  std::string hostport = "3010";
  std::string username = "default";

  std::string port = "3010";
    
  int opt = 0;
  while ((opt = getopt(argc, argv, "h:u:k:")) != -1){
    switch(opt) {
    case 'h':
      hostname = optarg;break;
    case 'u':
      username = optarg;break;
    // case 'p':
    //   port = optarg;break;
    case 'k':
      hostport=optarg;break;

    default:
      std::cout << "Invalid Command Line Argument\n";
    }
  }
  
  Client myc(hostname, username, hostport);
  
  signal(SIGINT, sig_ignore);

  myc.run();
  
  return 0;
}
