#ifndef SOCKET_CLIENT_SIDE_H
#define SOCKET_CLIENT_SIDE_H

#include "QemuSocketUtils.h"

#include <string>

namespace communication {

class ClientSocket {
	public:
		ClientSocket(std::string address, unsigned int port);
		~ClientSocket();
		int Init();
		void BuildLoadPluginMsg(SockMessage *msg, Plugins plugin_name, std::string start, std::string end);
		void BuildUnloadPluginMsg(SockMessage *msg, unsigned int idx);
		SockError SendCommand(SockMessage *msg);
		SockError ReceiveReply(SockMessage *msg);
		void CloseConnection();
	private:
		int sockfd = -1;
		std::string sockaddr;
		unsigned int sockport;

}; // class ClientSocket


} // namespace communication


#endif // SOCKET_CLIENT_SIDE_H
