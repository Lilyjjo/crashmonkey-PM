#include <iostream>
#include <fstream>
#include <vector>
#include <map>

#include "panda/plugin.h"

//for shared memory region
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

using namespace boost::interprocess;

static const int WRITE = 0;
static const int FLUSH = 1;
static const int FENCE = 2;


//TODO figure out how to put write_data_st into a header somewhere, rn it's also declared in writetracker.cpp
struct write_data_st {
    char data[8];
    size_t data_length;
    bool is_flushed;
};

typedef allocator<std::pair<char * const, struct write_data_st>, managed_shared_memory::segment_manager>
      ShmemAllocator;

typedef map< char *, struct write_data_st, std::less<char *>, ShmemAllocator> MyMap;

MyMap* mount_map;
MyMap* workload_map;

managed_shared_memory segment;


// method to print out map, can only be called after shared memory access is set up
__attribute__((unused))
static void print_snapshot_map(MyMap *snapshot_map) {

	std::cout << "\n\n***** printing Snapshot Map *****\n"<< std::endl;
	for (auto it = snapshot_map->begin(); it != snapshot_map->end(); ++it) {
		std::cout << (target_ulong) it->first << " => size: " << it->second.data_length << " data: " << it->second.data << std::endl;
	}
	std::cout << "\nEnd of snapshot map\n" << std::endl;
}

void replay_map(MyMap *map, target_ulong base) {

    for (auto map_iterator = map->begin(); map_iterator != map->end(); ++map_iterator) {
        if(map_iterator->second.data_length != 0){
          panda_physical_memory_rw((uint64_t) base + (uint64_t) map_iterator->first, (uint8_t *) map_iterator->second.data, map_iterator->second.data_length, true); 
        }
      }  
}


extern "C" bool init_plugin(void *self) {
    panda_arg_list *args = panda_get_args("replayer_map");
    auto base = panda_parse_ulong_opt(args, "base", 0x40000000, "Base physical address to replay at");
    std::cout << "replayer_map starting at \n" << std::hex << base << std::endl;

    std::cout << "about to open managed_shared_memory\n" << std::hex << base << std::endl;
    segment = managed_shared_memory(open_only, "MySharedMemory2");
    std::pair<MyMap*, managed_shared_memory::size_type> res;

    res = segment.find<MyMap> ("mount_map");
    mount_map = res.first;
    workload_map = segment.find<MyMap> ("workload_map").first; 

    std::cout << "shared memory opened \n" << std::hex << base << std::endl;

    // print_snapshot_map();

    std::cout << "about to put stuff into panda\n" << std::endl;
    
    replay_map(mount_map, base);
    replay_map(workload_map, base);

    std::cout << "replayer done, change made outside of vm" << std::endl;
    shared_memory_object::remove("MySharedMemory2");
    std::cout << "removed shared memory" << std::endl;
    return true;
}


extern "C" void uninit_plugin(void *self) {}
