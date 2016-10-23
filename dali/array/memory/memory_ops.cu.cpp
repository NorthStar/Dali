#include "dali/array/memory/memory_ops.h"

#include <mshadow/tensor.h>

#include "dali/array/memory/device.h"
#include "dali/utils/assert2.h"
#include "dali/utils/print_utils.h"

using utils::assert2;

// must be 1 byte long
typedef uint8_t alloc_type;

namespace memory {

#ifdef DALI_USE_CUDA
        mshadow::Tensor<mshadow::gpu, 2, alloc_type> dummy_gpu(void* ptr, int total_memory, int inner_dimension) {
            return mshadow::Tensor<mshadow::gpu, 2, alloc_type>((alloc_type*)ptr, mshadow::Shape2(
                    total_memory / inner_dimension, inner_dimension));
        }
#endif



    mshadow::Tensor<mshadow::cpu, 2, alloc_type> dummy_cpu(void* ptr, int total_memory, int inner_dimension) {
         return mshadow::Tensor<mshadow::cpu, 2, alloc_type>((alloc_type*)ptr, mshadow::Shape2(
                total_memory / inner_dimension, inner_dimension));
    }




    DevicePtr allocate(Device device, int amount, int inner_dimension) {
        if (device.is_cpu()) {
            auto dummy = dummy_cpu(NULL, amount, inner_dimension);
            mshadow::AllocSpace(&dummy, false);
            return DevicePtr(device, dummy.dptr_);
        }
#ifdef DALI_USE_CUDA
        else if (device.is_gpu()) {
            device.set_cuda_device();
            auto dummy = dummy_gpu(NULL, amount, inner_dimension);
            mshadow::AllocSpace(&dummy, false);
            return DevicePtr(device, dummy.dptr_);
        }
#endif
        else {
            ASSERT2(false, "Wrong device passed to Device enum");
            return DevicePtr(Device::device_of_doom(), (void*)NULL);
        }
    }

    void free(DevicePtr dev_ptr, int amount, int inner_dimension) {
        if (dev_ptr.device.is_cpu()) {
            auto dummy = dummy_cpu(dev_ptr.ptr, amount, inner_dimension);
            mshadow::FreeSpace(&dummy);
        }
#ifdef DALI_USE_CUDA
        else if (dev_ptr.device.is_gpu()) {
            dev_ptr.device.set_cuda_device();
            auto dummy = dummy_gpu(dev_ptr.ptr, amount, inner_dimension);
            mshadow::FreeSpace(&dummy);
        }
#endif
        else {
            ASSERT2(false, "Wrong device type passed to Device enum");
        }
    }

    void clear(DevicePtr dev_ptr, int amount, int inner_dimension) {
        if (dev_ptr.device.is_cpu()) {
            auto dummy = dummy_cpu(dev_ptr.ptr, amount, inner_dimension);
            dummy = (alloc_type)0;
        }
#ifdef DALI_USE_CUDA
        else if (dev_ptr.device.is_gpu()) {
            dev_ptr.device.set_cuda_device();
            auto dummy = dummy_gpu(dev_ptr.ptr, amount, inner_dimension);
            dummy = (alloc_type)0.0;
        }
#endif
        else {
            ASSERT2(false, "Wrong device passed to Device enum");
        }
    }

    void copy(DevicePtr dest, DevicePtr source, int amount, int inner_dimension) {
        if (dest.device.is_cpu() && source.device.is_cpu()) {
            auto dummy_dest   = dummy_cpu(dest.ptr,   amount, inner_dimension);
            auto dummy_source = dummy_cpu(source.ptr, amount, inner_dimension);
            mshadow::Copy(dummy_dest, dummy_source);
        }
#ifdef DALI_USE_CUDA
        else if (dest.device.is_cpu() && source.device.is_gpu()) {
            source.device.set_cuda_device();
            auto dummy_dest   = dummy_cpu(dest.ptr,   amount, inner_dimension);
            auto dummy_source = dummy_gpu(source.ptr, amount, inner_dimension);
            mshadow::Copy(dummy_dest, dummy_source);
        }
        else if (dest.device.is_gpu() && source.device.is_cpu()) {
            dest.device.set_cuda_device();
            auto dummy_dest   = dummy_gpu(dest.ptr,   amount, inner_dimension);
            auto dummy_source = dummy_cpu(source.ptr, amount, inner_dimension);
            mshadow::Copy(dummy_dest, dummy_source);
        }
        else if (dest.device.is_gpu() && source.device.is_gpu()) {
            dest.device.set_cuda_device();
            ASSERT2(dest.device.number() == source.device.number(),
                    "GPU -> GPU memory movement not supported yet.");
            auto dummy_dest   = dummy_gpu(dest.ptr,   amount, inner_dimension);
            auto dummy_source = dummy_gpu(source.ptr, amount, inner_dimension);
            mshadow::Copy(dummy_dest, dummy_source);
        }
#endif
        else {
            ASSERT2(false, "Wrong device passed to Device enum");
        }

    }

#ifdef DALI_USE_CUDA
        size_t cuda_available_memory() {
            size_t free_memory;
            size_t total_memory;
            cudaMemGetInfo(&free_memory, &total_memory);
            return free_memory;
        }
#endif
}
