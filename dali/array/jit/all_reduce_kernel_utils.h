#ifndef DALI_ARRAY_JIT_ALL_REDUCE_KERNEL_UTILS_H
#define DALI_ARRAY_JIT_ALL_REDUCE_KERNEL_UTILS_H
#include <string>

std::string create_all_reduce_kernel_caller(int ndim);
std::string create_argument_all_reduce_kernel_caller(int ndim);
std::string create_axis_reduce_kernel_caller(int ndim);
std::string create_warp_axis_reduce_kernel_caller(int ndim);
std::string create_argument_axis_reduce_kernel_caller(int ndim);

#endif  // DALI_ARRAY_JIT_ALL_REDUCE_KERNEL_UTILS_H
