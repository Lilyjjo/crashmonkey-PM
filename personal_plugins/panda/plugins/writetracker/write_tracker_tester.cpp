
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <map>


using namespace std;


struct write_data_st {
    char * data;
    bool is_flushed;
};

std::map<char *, write_data_st*> snapshot_map;

static void log_output(uint32_t pc, uint32_t offset, uint32_t write_size, void* write_data) {
  //output->write(reinterpret_cast<char*>(&pc), sizeof(pc));
  //output->write(reinterpret_cast<char*>(&type), sizeof(type));
 // switch (type) {
  //case WRITE: {

    // ******* New code:

    std::cout << "Inside log_output\n";

    int size = *reinterpret_cast<char *>(write_size);
    write_data_st * wdst = (struct write_data_st *) malloc(sizeof(struct write_data_st));
    wdst->data = (char *) malloc(size);
    memcpy(wdst->data, reinterpret_cast<char*>(write_data), size);
    snapshot_map.insert(std::pair<char *, struct write_data_st *>(reinterpret_cast<char*>(&offset), wdst));


    std::cout << "done with output\n";
    // **********



  //  output->write(reinterpret_cast<char*>(&offset), sizeof(offset));
  //  output->write(reinterpret_cast<char*>(&write_size), sizeof(write_size));
 //   output->write(reinterpret_cast<char*>(write_data), write_size);
 /*   ofs << "\nWrite ins " << offset << "\n";
    break;
  }
  case FLUSH: {
  //  output->write(reinterpret_cast<char*>(&offset), sizeof(offset));
    break;
  }
  default:
  case FENCE:
   // ofs << "\nFence ins";
    break;
  } */
}


static void print_snapshot_map() {

    std::cout << "\n\n***** Printing Snapshot Map *****\n";
    std::map<char *, struct write_data_st *>::iterator it;
    for ( it = snapshot_map.begin(); it != snapshot_map.end(); ++it) {
 	std::cout << it->first << " => " << it->second << "\n";
    } 	
    std::cout << "\nEnd of snapshot map\n";
}

int main(int argc, char** argv) {

	uint32_t size = 8;
	//char* write_data_fake = (char *) malloc(size);
	//* write_data_fake = "hello\n";

	char write_data_fake[] = "hello\n";

	std::cout << "fake data: " << write_data_fake << " \n";


	log_output(0, 0x40c8d8, size, (void *) &write_data_fake);



	/*
	static void log_output(target_ulong pc, event_type type, target_ulong offset, target_ulong write_size, void* write_data)
	[pc 0xffffffff9ada83cf] write to offset 40c8d8 addr 4840c8d8, size 8
	[pc 0xffffffff9ada83e4] write to offset 40c8e0 addr 4840c8e0, size 8
	[pc 0xffffffff9ada83e9] write to offset 40c8e8 addr 4840c8e8, size 8
	[pc 0xffffffff9ada83ee] write to offset 40c8f0 addr 4840c8f0, size 8
	*/

}