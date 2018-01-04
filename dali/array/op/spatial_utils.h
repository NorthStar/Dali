#ifndef DALI_ARRAY_OP_SPATIAL_UTILS_H
#define DALI_ARRAY_OP_SPATIAL_UTILS_H

#include <string>
#include <vector>
#include "dali/array/op/spatial_enums.h"

namespace op {
    void check_data_format(const std::string& data_format,
                           int* n_dim,
                           int* c_dim,
                           int* h_dim,
                           int* w_dim);

    struct SlidingWindowFunctionInfo {
        int out_w;
        int out_h;
        int batch_size;
        int in_channels;
        int in_h;
        int in_w;
        int padding_h;
        int padding_w;
        int odd_padding_h;
        int odd_padding_w;
    };

    struct PoolFunctionInfo : SlidingWindowFunctionInfo {
    };

    struct ConvFunctionInfo : SlidingWindowFunctionInfo {
        int filter_h;
        int filter_w;
        int out_channels;
    };

    PoolFunctionInfo compute_pool_info(
        const std::vector<int>& input_shape,
        const int& window_h,
        const int& window_w,
        const int& stride_h,
        const int& stride_w,
        const PADDING_T& padding,
        const std::string& data_format);

    ConvFunctionInfo compute_conv2d_info(
        const std::vector<int>& input_shape,
        const std::vector<int>& filters_shape,
        const int& stride_h,
        const int& stride_w,
        const PADDING_T& padding,
        const std::string& data_format);
}  // namespace op

#endif
