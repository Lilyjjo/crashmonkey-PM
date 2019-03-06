#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <sys/ioctl.h>

#include "ClientSocket.h"

#define BUFLEN 4096

namespace communication {

using std::string;
using std::cout;
using std::endl;
using std::to_string;

ClientSocket::ClientSocket(string addr, unsigned int port): sockaddr(addr), sockport(port) {};

//close sonnection on destructor
ClientSocket::~ClientSocket() {
	CloseConnection();
}

// Given a remote ip and port, connect the client to the socket
int ClientSocket::Init() {
    	struct sockaddr_in server_addr; 

	// Communicate over TCP, IPv4
	if (( sockfd = socket( AF_INET, SOCK_STREAM, 0 )) < 0) 
    	{	 
        	cout << "Socket creation error" << endl; 
        	return -1; 
    	} 
   
    	memset(&server_addr, '0', sizeof(server_addr)); 
   	server_addr.sin_family = AF_INET; 
    	server_addr.sin_port = htons(sockport); 

    	// Convert IPv4 addresses from string to address struct in af family 
    	if ( inet_pton( AF_INET, sockaddr.c_str(), &server_addr.sin_addr ) <=0 )  
    	{ 
        	cout << "Invalid address" << endl; 
        	return -1; 
    	} 

	// Now that we've populated address and port, let's connect to the Qemu
	// socket listening on port *sockport*
	// TODO (jaya) : This is a blocking connect operation. The current timeout is too large
	// Need to use select() in non-blocking mode to reduce the timeout.
    	if ( connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr) ) < 0 ) 
    	{ 
        	cout << "Connection Failed" << endl;; 
        	return -1; 
    	} 
	return 0;
}

void ClientSocket::BuildLoadPluginMsg(SockMessage& msg, Plugins plugin_name, string start, string end ) {
	msg.q_cmd = cLoadPlugin;
	msg.need_response = false;
	msg.q_cmd_options.plugin_name =plugin_name;
	msg.q_cmd_options.start = start;
	msg.q_cmd_options.end = end;
}

void ClientSocket::BuildLoadPluginMsgMapTracker(SockMessage& msg, Plugins plugin_name, string start, string end, string memory_name, string map_name) {
	msg.q_cmd = cLoadPlugin;
	msg.need_response = false;
	msg.q_cmd_options.plugin_name =plugin_name;
	msg.q_cmd_options.start = start;
	msg.q_cmd_options.end = end;
	msg.q_cmd_options.map_name = map_name;
	msg.q_cmd_options.memory_name = memory_name;
}

void ClientSocket::BuildLoadPluginMsgMapReplay(SockMessage& msg, Plugins plugin_name, string start, string memory_name, string map_name) {
	msg.q_cmd = cLoadPlugin;
	msg.need_response = false;
	msg.q_cmd_options.plugin_name =plugin_name;
	msg.q_cmd_options.start = start;
	msg.q_cmd_options.map_name = map_name;
	msg.q_cmd_options.memory_name = memory_name;
}


void ClientSocket::BuildUnloadPluginMsg(SockMessage& msg, unsigned int idx) {
	msg.q_cmd = cUnloadPlugin;
	msg.need_response = false;
	msg.q_cmd_options.id = idx;
}

SockError ClientSocket::SendCommand(const SockMessage& msg) {
	SockError err = eNone;
	
	// First check if the client is connected
	if ( sockfd < 0 ) {
		cout << "Client not connected" << endl;
		err = eNotConnected;
		return err;
	}
	
	// Get the command and the arguments
	QemuCommand cmd = msg.q_cmd;
	CommandOpts options = msg.q_cmd_options;

	// Based on the command, build the actual message now
	string complete_command;
	bool isCustomStart = false;
	switch(cmd) {
		case cListPlugins : 
			complete_command = "list_plugins";
			break;
		case cLoadPlugin : 
			complete_command = "load_plugin ";
			// Let's see how to handle the available plugins
			switch(options.plugin_name) {
				case pWritetracker:
					complete_command += "writetracker";
					if (! options.start.empty()) {
						complete_command += " start=";
						complete_command += options.start;
						isCustomStart = true;
					}
					if (! options.end.empty()) {
						if (isCustomStart)
							complete_command += ",";
						complete_command += "end=";
						complete_command += options.end;
					}
					if (! options.map_name.empty()) {
						if (isCustomStart)
							complete_command += ",";
						complete_command += "map_name=";
						complete_command += options.map_name;
					}
					if (! options.memory_name.empty()) {
						if (isCustomStart)
							complete_command += ",";
						complete_command += "memory_name=";
						complete_command += options.memory_name;
					}
					break;

				case pMounttracker:
					complete_command += "mount_tracker";
					if (! options.start.empty()) {
						complete_command += " start=";
						complete_command += options.start;
						isCustomStart = true;
					}
					if (! options.end.empty()) {
						if (isCustomStart)
							complete_command += ",";
						complete_command += "end=";
						complete_command += options.end;
					}
					if (! options.map_name.empty()) {
						if (isCustomStart)
							complete_command += ",";
						complete_command += "map_name=";
						complete_command += options.map_name;
					}
					if (! options.memory_name.empty()) {
						if (isCustomStart)
							complete_command += ",";
						complete_command += "memory_name=";
						complete_command += options.memory_name;
					}
					break;
				case pReplay:
					complete_command += "replayer";
					if (! options.start.empty()) {
						complete_command += " base=";
						complete_command += options.start;
					}
					break;
				case pReplayMap:
					complete_command += "replayer_map";
					if (! options.start.empty()) {
						complete_command += " base=";
						complete_command += options.start;
						isCustomStart = true;
					}
					if (! options.memory_name.empty()) {
						if (isCustomStart)
							complete_command += ",";
						complete_command += "memory_name=";
						complete_command += options.memory_name;
					}
					if (! options.map_name.empty()) {
						if (isCustomStart)
							complete_command += ",";
						complete_command += "map_name=";
						complete_command += options.map_name;
					}
					break;
				default:
					cout << "Undefined Plugin" << endl;
					return eOther;
			}
			break;
		case cUnloadPlugin : 
			complete_command = "unload_plugin ";
			complete_command += to_string(options.id);
			break;
		default :
			cout << "Undefined qemu command" << endl;
			return eOther;
	}

	// The command doesn't execute at the monitor without a newline
	complete_command += "\n";
	cout << "Command being sent is : " << complete_command << endl;
	if ( send( sockfd, complete_command.c_str(), complete_command.length(), 0) < 0) {
		err = eOther;
		return err;
	}
	return err;
}

SockError ClientSocket::ReceiveReply(SockMessage& msg) {
	SockError err = eNone;
	int valread;
	char buffer[BUFLEN];
	int bytes_read = 0;
	while(1) {
		valread = recv( sockfd, buffer, BUFLEN, 0);
    		if (valread <= 0){
			err = eOther;
			return err;
		}
		bytes_read += valread;
		string compare(buffer);
		//cout  << "Reply : " << buffer << " Bytes read = " << compare.find("cmEOF") << endl;
		string::size_type n = compare.find("cmEOF");
		if (n != string::npos){
			break;
		}
	}
	return err;
}

void ClientSocket::CloseConnection() {
	close(sockfd);
	sockfd = -1;
}

} //namespace communication
