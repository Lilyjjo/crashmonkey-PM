#include <unistd.h>
#include <string>
#include <iostream>
#include <getopt.h>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <fstream>

#include "ClientSide.h"

#define OPTIONS_LIST "b:e:i:p:v"

// By default the writetracker plugin will trace from 1GB - 1GB+128MB
#define DEFAULT_START_ADDR "0x40000000"
#define DEFAULT_END_ADDR "0x48000000"

using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::left;
using std::setw;
using std::time;
using std::localtime;
using std::put_time;
using std::ostringstream;
using std::ofstream;

using namespace communication;

static bool verbose_flag;

static struct option harness_options[] = {
	{ "begin-trace-addr", required_argument, NULL, 'b' },
	{ "end-trace-addr", required_argument, NULL, 'e' },
	{ "ip", required_argument, NULL, 'i' },
	{ "port", required_argument, NULL, 'p' },
	{ "verbose", no_argument, NULL, 'v' },
	{ 0, 0, 0, 0 },
};

int main(int argc, char** argv) {

	// Let's set some default values if the user doesn't provide args
	string remote_ip("127.0.0.1");
	unsigned int remote_port = 4444;
	string begin_trace_addr(DEFAULT_START_ADDR);
	string end_trace_addr(DEFAULT_END_ADDR);

	// Parse inputs
	int option_index = 0;
	while (1) {
		int ret = getopt_long(argc, argv, OPTIONS_LIST, harness_options, &option_index);
		
		// if all command line options are parsed, exit the loop
		if ( ret == -1 ) {
			break;
		}

		switch (ret) {
			case 'b':
				begin_trace_addr = string(optarg);
				break;
			case 'e':
				end_trace_addr = string(optarg);
				break;
			case 'i':
				remote_ip = string(optarg);
				break;
			case 'p':
				remote_port = atoi(optarg);
				break;
			case 'v':
				verbose_flag = true;
				break;
			case '?':
			case ':':
			default :
				cerr << "Error parsing arguments" << endl;
				return -1;		
		}
	}

	// Print the settings in which CrashMonkey-PM would run
	cout << "\n******** Running CrashMonkey-PM **********\n" << endl;
	cout << setw(30) << left << "Remote IP address" << setw(2) << left << ":"
	     << setw(2) << left << remote_ip << endl; 

	cout << setw(30) << left << "Remote port" << setw(2) << left << ":"
	     << setw(2) << left << remote_port << endl; 
	
	cout << setw(30) << left << "Tracking from memory address" << setw(2) << left << ":"
	     << setw(2) << left << begin_trace_addr << endl; 

	cout << setw(30) << left << "Tracking until memory address" << setw(2) << left << ":"
	     << setw(2) << left << end_trace_addr << endl; 

	cout << "\n******************************************\n" << endl;
	
	// create a log file
    	auto t = time(nullptr);
    	auto tm = *localtime(&t);
	ostringstream os;
	os << put_time(&tm, "%d%b%y_%T");
	string s = "CM_" + os.str() + ".log";

	ofstream log_file(s);

	/***********************************************************
	* 1. Connect to the Qemu Monitor
	************************************************************/

	ClientSocket* vm = NULL;
	vm = new ClientSocket(remote_ip, remote_port);
	
	//initialize and connect to socket 
	if (vm->Init() < 0) {
		int err_no = errno;
		cout << "Error starting socket" << endl;
		delete vm;
		return -1;
	}

	cout << "Connected to socket" << endl;
	log_file << "Connected to socket" << endl;


	/***********************************************************
	* 2. Load the writetracker plugin
	* Build the command to be sent over socket
	* Send the command
	*	Now this enables mem tracing within the ranges 
	*	specified.
	* (TODO): Is it worth waiting for the reply, which is simply 
	* 	an echo of the command we sent? Probably grep the 
	* 	reply string for error messages? But I need to
	* 	insert a sec of sleep to read the contents  
	************************************************************/


	SockMessage *msg = new SockMessage();
	vm->BuildLoadPluginMsg(msg, pWritetracker, begin_trace_addr, end_trace_addr);
	
	if (vm->SendCommand(msg) != eNone ) {
		int err_no = errno;
		cout << "Error sending message" << endl;
		delete vm;
		return -1;
	}
	sleep(1);
	vm->ReceiveReply(msg);

	/***********************************************************
	* 3. Execute the workload
	************************************************************/


	//TODO
	

	/***********************************************************
	* 4. UnLoad the writetracker plugin
	* Build the command to be sent over socket
	* Send the command
	* 	This will stop tracing and the results of the output
	* 	will be in a file named wt.out on the remote host.  
	************************************************************/


	cout << "Waiting before unoad.." << endl;
	sleep(5);

	msg = new SockMessage();
	vm->BuildUnloadPluginMsg(msg, 0);
	
	if (vm->SendCommand(msg) != eNone ) {
		int err_no = errno;
		cout << "Error sending message" << endl;
		delete vm;
		return -1;
	}
	sleep(1);
	vm->ReceiveReply(msg);


	/***********************************************************
	* 5. Load the replay plugin
	* 	This plugin should replay the serialized 
	* 	traces in the wt.out file at the host
	*	starting at memory addr denoted by <end>
	************************************************************/


	/***********************************************************
	* 6. Unload the replay plugin
	************************************************************/

	/***********************************************************
	* 7. Perform consistency tests
	************************************************************/
	

	/***********************************************************
	* 8. Cleanup and exit
	************************************************************/

	delete msg;
	
	log_file.close();
	delete vm;
	return 0;


}