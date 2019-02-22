#include <iostream>
#include <fstream>
#include <vector>
#include <map>

#include "panda/plugin.h"

static const int WRITE = 0;
static const int FLUSH = 1;
static const int FENCE = 2;

//TODO figure out how to put write_data_st into a header somewhere, rn it's also declared in writetracker.cpp
struct write_data_st {
    char * data;
    size_t data_length;
    bool is_flushed;
};

std::map<char *, write_data_st*> snapshot_map;


static void fill_fake_snapshot_map() {

    std::cout << "***** filling fake snapshot_map *****\n"<< endl;
    std::map<char *, struct write_data_st *>::iterator it;
    size_t write_size = 8;
    char data[] = "********";
    char * offset = 0x40000000;

    for (int i = 0, i < 100, i++) {
      write_data_st * wdst = (struct write_data_st *) malloc(sizeof(struct write_data_st));
      wdst->data = (char *) malloc(write_size);
      wdst->data_length = (size_t) write_size;
      wdst->is_flushed = false;
      memcpy(wdst->data, *data, write_size);
      snapshot_map.insert(std::pair<char *, struct write_data_st *>(reinterpret_cast<char*>(offset + (i * write_size)), wdst));
    }   
    std::cout << "***** done filling fake map *****\n" << endl;
}


extern "C" bool init_plugin(void *self) {
    panda_arg_list *args = panda_get_args("replayer_map");
    auto base = panda_parse_ulong_opt(args, "base", 0x40000000, "Base physical address to replay at");
    //auto file = panda_parse_string(args, "file", "wt.out");
    
    std::cout << "replayer_map starting at " << std::hex << base << std::endl;

    uint64_t pc;
    int type;
    uint64_t offset;
    uint64_t write_size;
    std::vector<uint8_t> write_data;

    std::map<char *, struct write_data_st *>::iterator map_iterator;

    for (map_iterator = snapshot_map.begin(); map_iterator != snapshot_map.end(); ++map_iterator) {
       write_data_st* wdst = map_iterator->second;
        char * offset2 = map_iterator->first;
        if(wdst->data != NULL){
          panda_physical_memory_rw(base + offset2, wdst->data, wdst->offset, true); 
        }
      }  

   std::cout << "replay_map done" << std::endl;
   return true;
}




extern "C" void uninit_plugin(void *self) {}
