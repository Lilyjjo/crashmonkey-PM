#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "Tester.h"

#define TEST_CLASS_FACTORY "test_case_get_instance"
#define TEST_CLASS_DEFACTORY "test_case_delete_instance"

#define MNT_POINT_RECORD "/mnt/pmem0"
#define MNT_POINT_REPLAY "/mnt/pmem1"
#define MNT_POINT_SNAPSHOT "/mnt/ramdisk"
#define SNAPSHOT_NAME "snapshot.snap"

// We will assume for now that we snapshot only at the final checkpoint
// This is due to the testing strategy we employ
// Since we test all workloads of seq-length 1, followed by 2 and so on, the
// checkpoints other than the final one, would have been tested by some workload
// of lower length. 
#define NUM_SNAPSHOTS 1

using std::calloc;
using std::cerr;
using std::chrono::steady_clock;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::time_point;
using std::cout;
using std::endl;
using std::free;
using std::ifstream;
using std::ios;
using std::ostream;
using std::ofstream;
using std::pair;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;

using tests::test_create_t;
using tests::test_destroy_t;
using wrapper::DiskMod;

Tester::Tester(const unsigned int dev_size, const bool verbosity)
	: device_size(dev_size), verbose(verbosity) {
//  	snapshot_path_ = "/dev/cow_ram_snapshot1_0";
}

Tester::~Tester() {
  	if (fs_commands_ != NULL) {
    		delete fs_commands_;
  }
}

void Tester::set_fs(const string fs_type) {
  	fs = fs_type;
  	fs_commands_ = GetFSCommands(fs);
	assert(fs_commands_ != NULL);
}

void Tester::set_record_device(const string device_path) {
	record_device_path = device_path;
}

void Tester::set_replay_device(const string device_path) {
	replay_device_path = device_path;
}

void Tester::StartTestSuite() {
  	// Construct a new element at the end of our vector.
  	test_results_.emplace_back();
  	current_test_suite_ = &test_results_.back();
}

void Tester::EndTestSuite() {
 	 current_test_suite_ = NULL;
}


int Tester::snapshot_device() {
	cout << "Creating snapshot of " << record_device_path << endl;
	string command = "dd if=" + record_device_path + " of="
	       	+ MNT_POINT_SNAPSHOT + "/" + SNAPSHOT_NAME + " bs=1M "
		+ "count=" + to_string(device_size);
	cout << "Snapshot dev command is : " << command << endl;
	if (system(command.c_str()) != 0) {
		return SNAP_ERR;
	}
	return SUCCESS;
}

int Tester::restore_snapshot() {
	cout << "Restoring snapshot of " << record_device_path 
		<< " to " << replay_device_path << endl;
	string command = "dd if=" MNT_POINT_SNAPSHOT "/" SNAPSHOT_NAME 
		" of=" + replay_device_path + " bs=1M count=" + to_string(device_size);
	cout << "Restore snapshot dev command is : " << command << endl;
	if (system(command.c_str()) != 0 ) {
		return RESTORE_ERR;
	}
	return SUCCESS;
}

int Tester::mount_device(string device_path, string mount_point) {
	string command = fs_commands_->GetMountCommand(device_path, mount_point);
	if (system(command.c_str()) != 0) {
		return MNT_ERR;
	}
	return SUCCESS;
}

int Tester::format_and_mount_device(string device_path, string mount_point) {
	string command = fs_commands_->GetMkfsCommand(device_path, mount_point);
	if (system(command.c_str()) != 0) {
		return FMT_ERR;
	}
	return SUCCESS;
}

int Tester::umount_device() {
		if (umount(MNT_POINT_RECORD) < 0) {
			record_dev_mounted = true;
			return UMNT_ERR;
		}
	record_dev_mounted = false;
	return SUCCESS;
}

int Tester::test_setup() {
	return test_loader.get_instance()->setup();
}

int Tester::test_init_values(string mount_dir, long filesys_size) {
	return test_loader.get_instance()->init_values(mount_dir, filesys_size);
}

int Tester::test_run(const int change_fd, const int checkpoint) {
	return test_loader.get_instance()->Run(change_fd, checkpoint);
}

// The device_path is that of the replay device
vector<milliseconds> Tester::test_check(const string device_path, ofstream &log) {
	assert(current_test_suite_ != NULL);
	vector<milliseconds> res(3, duration<int, std::milli>(-1));
	SingleTestInfo test_info;
	unsigned int test_num = 1;
	test_info.test_num = test_num++;

	// This funstion will be called after the replay i.e. 
	// the replay_device is in an unmounted state.
	//
	// So first check if the filesystem is mountable

	cout << "In test_check, about to run consistnecy checks" << endl;
	
	time_point<steady_clock> mount_start_time = steady_clock::now();
	if (mount_device(device_path, MNT_POINT_REPLAY) != SUCCESS) {
		test_info.fs_test.SetError(FileSystemTestResult::kKernelMount);
		cout << "error with mounting replay device! exiting test_check" << endl;
		return res;
	}

	if (mount_device(record_device_path, MNT_POINT_RECORD) != SUCCESS) {
		test_info.fs_test.SetError(FileSystemTestResult::kKernelMount);

		cout << "error with mounting record device! exiting test_check" << endl;
		return res;
	}

	cout << "Mounted both record and replay device for checks" << endl;
	system("mount | grep pmem");
	system("ls /mnt/pmem0");
	system("ls /mnt/pmem1");
	cout << "Checking using system diff command" << endl;
	system("diff -qr /mnt/pmem0 /mnt/pmem1");


	time_point<steady_clock> mount_end_time = steady_clock::now();
	res.at(2) = duration_cast<milliseconds>(mount_end_time - mount_start_time);	

	// Once mounted, begin testing
	time_point<steady_clock> test_case_start_time = steady_clock::now();
	bool retVal = check_disk_and_snapshot_contents();
	if (!retVal) {
		test_info.data_test.SetError(tests::DataTestResult::kAutoCheckFailed);
	} 
	time_point<steady_clock> test_case_end_time = steady_clock::now();
	res.at(1) = duration_cast<milliseconds>(test_case_end_time - test_case_start_time);

	test_info.PrintResults(log);
	current_test_suite_->TallyTimingResult(test_info);
	return res;
}


bool Tester::check_disk_and_snapshot_contents() {
	
	// we will check only the last checkpoint
	// so the oracle is the record device, and the replay device
	// will be tested against it
	ofstream diff_file;
	diff_file.open("diff", std::fstream::out | std::fstream::app);
	DiskContents disk1(record_device_path, fs), disk2(replay_device_path, fs);
	disk1.set_mount_point(MNT_POINT_RECORD);
	disk2.set_mount_point(MNT_POINT_REPLAY);

	int num_mods = mods_.size() - 1;
	cout << "Num mods " << num_mods << endl;
	// get the file/dir corresponding to the last checkpoint operation
	for (auto last_mod : mods_.at(num_mods-1)) {
		cout << "last mod" << last_mod.mod_type << endl;
		if (last_mod.mod_type == DiskMod::kFsyncMod) {
			string path(last_mod.path);
			cout << "Last checkpoint was fsync to " << path << endl;
			path.erase(0, 10);
			std::cout << "File path : " << path << std::endl;
			bool ret = disk1.compare_entries_at_path(disk2, path, diff_file);
			cout << "Compare entries returned " << ret << endl;
			if (ret) {
				if (disk1.sanity_checks(diff_file) == false) {
					std::cout << "Failed: Sanity checks on " << replay_device_path << endl;
					return false;
				}
			}
			return ret;
		}
		else if (last_mod.mod_type == DiskMod::kSyncMod) {
			cout << "Last checkpoint was sync" << endl;
			bool ret = disk1.compare_disk_contents(disk2, diff_file);
			if (ret) {
				if (disk1.sanity_checks(diff_file) == false) {
					std::cout << "Failed: Sanity checks on " << replay_device_path << endl;
					return false;
				}
			}
			return ret;
		}
		else if (last_mod.mod_type == DiskMod::kDataMod || last_mod.mod_type == DiskMod::kSyncFileRangeMod) {
			string path(last_mod.path);
			cout << "Last checkpoint was fdatasync to " << path << endl;
			path.erase(0, 10);
			bool ret = disk1.compare_file_contents(disk2, path, last_mod.file_mod_location, 
				last_mod.file_mod_len, diff_file);
			if (ret) {
				if (disk1.sanity_checks(diff_file) == false) {
					std::cout << "Failed: Sanity checks on " << replay_device_path << endl;
					return false;
				}
			}
			return ret;
		}
	}
	cout << "ERROR: " << __func__ << endl;	
	return false;
}

int Tester::GetChangeData(const int fd) {
  // Need to read a 64-bit value, switch it to big endian to figure out how much
  // we need to read, read that new data amount, and add it all to a buffer.
  while (true) {
    // Get the next DiskMod size.
    uint64_t buf;
    const int read_res = read(fd, (void *) &buf, sizeof(uint64_t));
    if (read_res < 0) {
      return read_res;
    } else if (read_res == 0) {
      // No more data to read.
      break;
    }

    uint64_t next_chunk_size = be64toh(buf);

    // Read the next DiskMod.
    shared_ptr<char> data(new char[next_chunk_size], [](char *c) {delete[] c;});
    memcpy(data.get(), (void *) &buf, sizeof(uint64_t));
    unsigned long long int read_data = sizeof(uint64_t);
    while (read_data < next_chunk_size) {
      const int res = read(fd, data.get() + read_data,
          next_chunk_size - read_data);
      if (res <= 0) {
        // We shouldn't find a size for a DiskMod without the rest of the
        // DiskMod.
        return -1;
      }
      read_data += res;
    }

    DiskMod mod;
    const int res = DiskMod::Deserialize(data, mod);
    if (res < 0) {
      return res;
    }

    if (mod.mod_type == DiskMod::kCheckpointMod) {
      // We found a checkpoint, so switch to a new set of DiskMods.
      cout << "Found a checkpoint" << endl;
      mods_.push_back(vector<DiskMod>());
    } else {
      if (mods_.empty()) {
        // We're just starting, so give us a place to put the mods.
        mods_.push_back(vector<DiskMod>());
      }
      // Just append this DiskMod to the end of the last set of DiskMods.
      mods_.back().push_back(mod);
    }
  }

  return SUCCESS;
}

int Tester::CreateCheckpoint() {
	return SUCCESS;
}

int Tester::test_load_class(const char* path) {
	return test_loader.load_class<test_create_t *>(path, TEST_CLASS_FACTORY,
      TEST_CLASS_DEFACTORY);
}

void Tester::test_unload_class() {
	test_loader.unload_class<test_destroy_t *>();
}

void Tester::cleanup_harness() {
	int umount_res;
	int err;
	do {
		umount_res = umount_device();
		if (umount_res < 0) {
			err = errno;
			usleep(500);
		}
	}while (umount_res < 0 && err == EBUSY);

	if (umount_res < 0) {
		cerr << "Unable to unmount device" << endl;
		test_unload_class();
		return; 
	}

	test_unload_class();
	if (fs_commands_ != NULL) {
		delete fs_commands_;
		fs_commands_ = NULL;
	}
}

void Tester::PrintTestStats(std::ostream& os) {
	for (const auto& suite : test_results_) {
		suite.PrintResults(os);
	}
}

std::chrono::milliseconds Tester::get_timing_stat(time_stats timing_stat) {
	return timing_stats[timing_stat];
}

std::ostream& operator<<(std::ostream& os, Tester::time_stats time) {
	switch (time) {
		case Tester::SNAPSHOT_TIME:
			os << "snapshot restore time";
			break;
		case Tester::TEST_CASE_TIME:
			os << "test case time";
			break;
		case Tester::MOUNT_TIME:
			os << "mount/umount time";
			break;
		case Tester::TOTAL_TIME:
			os << "total time";
			break;
		default:
			os.setstate(std::ios_base::failbit);
	}
	return os;
}


