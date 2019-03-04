#ifndef QEMU_SOCKET_UTILS_H
#define QEMU_SOCKET_UTILS_H

#include <memory>

namespace communication {

// The client/guest VM would send Qemu monitor messages to the
// Qemu monitor server that is already listening.
// The response must be string values

typedef enum {
	cListPlugins,
	cLoadPlugin,
	cUnloadPlugin
} QemuCommand;

typedef enum {
	pWritetracker,
	pReplay,
	pReplayMap
}Plugins;

typedef struct {
	Plugins plugin_name;
	std::string start;
	std::string end;
	std::string map_name;
	std::string memory_name;
	unsigned int id;
}CommandOpts;


typedef enum {
	eNotConnected,
	eAlreadyConnected,
	eOther,
	eNone
} SockError;

struct SockMessage {
	QemuCommand q_cmd;
	bool need_response;
	CommandOpts q_cmd_options;
	//SockMessage() : q_cmd_options(new CommandOpts()) {}
};

} // namespace communication

#endif // QEMU_SOCKET_UTILS_H


