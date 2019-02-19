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

MyMap* snapshot_map;

managed_shared_memory segment;

static void print_snapshot_map() {

    std::cout << "\n\n***** printing Snapshot Map *****\n"<< std::endl;
    //std::map<char *, struct write_data_st *>::iterator it;
    for (auto it = snapshot_map->begin(); it != snapshot_map->end(); ++it) {
      std::cout << (target_ulong) it->first << " => size: " << it->second.data_length << " data: " << it->second.data << std::endl;
    }
    std::cout << "\nEnd of snapshot map\n" << std::endl;
}

/*
static void fill_fake_snapshot_map(char * base) {

    std::cout << "***** filling fake snapshot_map *****\n"<< std::endl;
    std::map<char *, struct write_data_st *>::iterator it;
    size_t write_size = 8;
    char data[] = "********";
    char * offset = base;

    for (int i = 0; i < 100; i++) {
      write_data_st * wdst = (struct write_data_st *) malloc(sizeof(struct write_data_st));
      wdst->data = (char *) malloc(write_size);
      wdst->data_length = (size_t) write_size;
      wdst->is_flushed = false;
      memcpy(wdst->data, data, write_size);
      snapshot_map.insert(std::pair<char *, struct write_data_st *>(reinterpret_cast<char*>(offset + (i * write_size)), wdst));
    }   
    std::cout << "***** done filling fake map *****\n" << std::endl;
} */


extern "C" bool init_plugin(void *self) {
    panda_arg_list *args = panda_get_args("replayer_map");
    auto base = panda_parse_ulong_opt(args, "base", 0x40000000, "Base physical address to replay at");
    //auto file = panda_parse_string(args, "file", "wt.out");
    //fill_fake_snapshot_map((char *) base);
    std::cout << "replayer_map starting at \n" << std::hex << base << std::endl;

    std::cout << "about to open managed_shared_memory\n" << std::hex << base << std::endl;

    segment = managed_shared_memory(open_only, "MySharedMemory1");

    snapshot_map = segment.find<MyMap> ("MySharedMemory1");

    std::pair<MyType*, managed_shared_memory::size_type> res;


    std::cout << "shared memory opened \n" << std::hex << base << std::endl;

    print_snapshot_map();

    std::cout << "about to put stuff into panda\n" << std::endl;


    //uint64_t pc;
    //int type;
   // uint64_t offset;
    //uint64_t write_size;
    std::vector<uint8_t> write_data;



    //auto map_iterator = snapshot_map->find(reinterpret_cast< char*>(offset));

    for (auto map_iterator = snapshot_map->begin(); map_iterator != snapshot_map->end(); ++map_iterator) {
        if(map_iterator->second.data_length != 0){
          panda_physical_memory_rw((uint64_t) base + (uint64_t) map_iterator->first, (uint8_t *) map_iterator->second.data, map_iterator->second.data_length, true); 
        }
      }  

   std::cout << "replayer done, change made outside of vm" << std::endl;
   return true;
}




extern "C" void uninit_plugin(void *self) {}
