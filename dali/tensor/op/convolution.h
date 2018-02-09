#ifndef DALI_TENSOR_OP_CONVOLUTION_H
#define DALI_TENSOR_OP_CONVOLUTION_H

#include "dali/tensor/Mat.h"
#include "dali/tensor/Tape.h"
#include "dali/utils.h"

template<typename R> class Mat;

namespace matops {
    template<typename R>
    struct Convolution{
        static Mat<R> conv2d(
            Mat<R> image,
            Mat<R> kernels,
            const std::vector<int>& image_shape,
            const int& kernel_height,
            const int& kernel_width,
            const int& kernel_stride);

        // As described in the "Neural Turing Machine" paper.
        static Mat<R> circular_convolution(Mat<R> input, Mat<R> shift);
    };
}

#endif
