#include "dali/array/memory/memory_bank.h"

#include <cuckoohash_map.hh>

#include "dali/config.h"
#include "dali/utils/assert2.h"

using memory_ops::Device;
using memory_ops::DevicePtr;
using std::vector;
using utils::assert2;

namespace memory_bank {
    const int INITIAL_HASHMAP_SIZE = 100000;
    struct DeviceBank {
        DeviceBank() : blobs(INITIAL_HASHMAP_SIZE), num_allocations(0), total_memory(0) {}
        cuckoohash_map<unsigned long long, std::vector<void*>> blobs;
        std::atomic<unsigned long long> num_allocations;
        std::atomic<unsigned long long> total_memory;
    };

    DeviceBank cpu_bank;
    DeviceBank gpu_bank;

    DeviceBank& get_bank(Device device) {
        if (device == memory_ops::DEVICE_CPU) {
            return cpu_bank;
        }
#ifdef DALI_USE_CUDA
        else if (device == memory_ops::DEVICE_GPU) {
            return gpu_bank;
        }
#endif
        else {
            assert2(false, "Wrong device passed to Device enum");
        }
    }


    void deposit(DevicePtr dev_ptr, int amount, int inner_dimension) {
        get_bank(dev_ptr.device).blobs.upsert(amount, [dev_ptr](std::vector<void*>& deposit_box) {
            deposit_box.emplace_back(dev_ptr.ptr);
        }, {dev_ptr.ptr});
    }

    DevicePtr allocate(Device device, int amount, int inner_dimension) {
        void* memory = NULL;
        auto& bank = get_bank(device);
        bool success = bank.blobs.update_fn(amount, [&memory](std::vector<void*>& deposit_box) {
            if (!deposit_box.empty()) {
                memory = deposit_box.back();
                deposit_box.pop_back();
            }
        });
        if (memory != NULL) {
            return DevicePtr(device, memory);
        } else {
            bank.num_allocations++;
            bank.total_memory += amount;
            return memory_ops::allocate(device, amount, inner_dimension);
        }
    }

    void clear(Device device) {
        auto& bank = get_bank(device);

        vector<int> amounts_to_clear;
        for (auto it = bank.blobs.cbegin(); !it.is_end(); it++) {
            amounts_to_clear.push_back(it->first);
        }
        for (auto amount: amounts_to_clear) {
            bank.blobs.update_fn(amount, [&bank, device, amount](std::vector<void*>& deposit_box) {
                for (auto ptr: deposit_box) {
                    memory_ops::free(DevicePtr(device, ptr), amount, 1);
                }
                bank.total_memory -= amount * deposit_box.size();
                deposit_box.clear();
            });
        }
    }

}