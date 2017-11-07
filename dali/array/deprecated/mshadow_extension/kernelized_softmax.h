#ifndef DALI_ARRAY_THRUST_SOFTMAX_TRANSPOSE_H
#define DALI_ARRAY_THRUST_SOFTMAX_TRANSPOSE_H
//#include "dali/array/op/impl/ops.h"
#include "dali/config.h"
#include <mshadow/tensor.h>

#ifdef DALI_USE_CUDA
#include <thrust/system/cuda/execution_policy.h>
#endif
/**
Thrust Softmax
--------------

GPU and CPU implementations of Softmax. Both operations add
support for temperature (controls the roll off of the
exponentiation during Softmax).

The column-wise softmax is done using Thrust, while the
row-wise softmax (`softmax_rowwise`) is achieved by
modifying one line from MShadow's version.
**/

namespace internal {
    #ifdef DALI_USE_CUDA
        template<int x_bits, typename R,  typename DstPlan, typename SrcPlan>
        __global__ void SoftmaxKernel(DstPlan dst, SrcPlan src, mshadow::index_t num_cols, const double temperature) {
            const unsigned buffer_size = 1 << x_bits;
            const int row = blockIdx.x;
            const int thread_idx = threadIdx.x;
            __shared__ R buffer[buffer_size];
            // step 1: get max
            if (thread_idx < num_cols) {
                buffer[thread_idx] = src.Eval(row, thread_idx);
            }
            for (unsigned x = buffer_size; x < num_cols; x += buffer_size) {
                const int col = x + thread_idx;
                if (col < num_cols) {
                    R a = src.Eval(row, col);
                    buffer[thread_idx] = max(a, buffer[thread_idx]);
                }
            }
            __syncthreads();
            // if number of rows is smaller than buffer,
            // fill buffer with copy of buffer[0] - this
            // makes sure reduction does not use uninitialized
            // values in the buffer and returns correct max.
            if (thread_idx >= num_cols) {
                buffer[thread_idx] = buffer[0];
            }
            __syncthreads();
            mshadow::cuda::ReduceX<mshadow::red::maximum, x_bits>(buffer, thread_idx);

            __syncthreads();
            // every thread memorizes max value in column,
            // so that we can reuse the buffer, for next
            // task
            R max_in_row = buffer[0];
            __syncthreads();
            // clear buffer (so that sum works out later)
            buffer[thread_idx] = 0.0f;
            __syncthreads();

            // calculate normalizer, with writeback
            for (unsigned x = 0; x < num_cols; x += buffer_size) {
                const int col = x + thread_idx;
                if (col < num_cols) {
                    R p = expf((src.Eval(row, col) - max_in_row) / temperature);
                    // add sum to buffer, so that we can later reduce it to
                    // column-wise sum of exps and use as normalizer.
                    buffer[thread_idx] += p;
                    // save exped value to the corresponding idx in destination.
                    dst.REval(row, col) = p;
                }
            }
            // calculate normalizer by reducing partial sums
            __syncthreads();
            mshadow::cuda::ReduceX<mshadow::red::sum, x_bits>(buffer, thread_idx);
            __syncthreads();
            R sum_in_row = buffer[0];

            for (unsigned x = 0; x < num_cols; x += buffer_size) {
                const int col = x + thread_idx;

                if (col < num_cols) {
                    dst.REval(row, col) /= sum_in_row;
                }
            }
        }

        template<int x_bits, typename R,  typename DstPlan, typename SrcPlan>
        __global__ void SoftmaxKernelCached(DstPlan dst, SrcPlan src, mshadow::index_t num_cols, const double temperature) {
            const unsigned buffer_size = 1 << x_bits;
            const int num_offsets = num_cols/buffer_size + 1;
            const int row = blockIdx.x;
            const int thread_idx = threadIdx.x;

            __shared__ R buffer[buffer_size];
            R row_cache[20];
            // step 0: copy the memory to cache.
            for (unsigned offset = 0; offset < num_offsets; ++offset) {
                const int col = offset * buffer_size + thread_idx;
                if (col < num_cols) {
                    row_cache[offset] = src.Eval(row, col);
                }
            }

            // step 1: get max
            if (thread_idx < num_cols) {
                buffer[thread_idx] = row_cache[0];
            }
            for (unsigned offset = 0; offset < num_offsets; ++offset) {
                const int col = offset * buffer_size + thread_idx;
                if (col < num_cols) {
                    buffer[thread_idx] = max(row_cache[offset], buffer[thread_idx]);
                }
            }
            __syncthreads();
            // if number of rows is smaller than buffer,
            // fill buffer with copy of buffer[0] - this
            // makes sure reduction does not use uninitialized
            // values in the buffer and returns correct max.
            if (thread_idx >= num_cols) {
                buffer[thread_idx] = buffer[0];
            }
            __syncthreads();
            mshadow::cuda::ReduceX<mshadow::red::maximum, x_bits>(buffer, thread_idx);

            __syncthreads();
            // every thread memorizes max value in column,
            // so that we can reuse the buffer, for next
            // task
            R max_in_row = buffer[0];
            __syncthreads();
            // clear buffer (so that sum works out later)
            buffer[thread_idx] = 0.0f;
            __syncthreads();

            // calculate normalizer, with writeback
            for (unsigned offset = 0; offset < num_offsets; ++offset) {
                const int col = offset * buffer_size + thread_idx;
                if (col < num_cols) {
                    const R p = expf((row_cache[offset] - max_in_row) / temperature);
                    // add sum to buffer, so that we can later reduce it to
                    // column-wise sum of exps and use as normalizer.
                    buffer[thread_idx] += p;
                    // save exped value to the corresponding idx in destination.
                    row_cache[offset] = p;
                }
            }
            // calculate normalizer by reducing partial sums
            __syncthreads();
            mshadow::cuda::ReduceX<mshadow::red::sum, x_bits>(buffer, thread_idx);
            __syncthreads();
            R sum_in_row = buffer[0];

            for (unsigned offset = 0; offset < num_offsets; ++offset) {
                const int col = offset * buffer_size + thread_idx;

                if (col < num_cols) {
                    dst.REval(row, col) = row_cache[offset] / sum_in_row;
                }
            }
        }

        // Note: in a dim3 (width, height, depth)
        // every uninitialized dimension defaults to 1.

        static const int MAX_ROW_SIZE_FOR_CACHED = 1000;

        // Note: <<<Dg, Db, Ns, S>>> CUDA Language Extension is explained here:
        // http://docs.nvidia.com/cuda/cuda-c-programming-guide/#execution-configuration
        template<typename R, template<class,int,class>class LeftT, template<class,int,class>class RightT>
        void softmax_rowwise(LeftT<mshadow::gpu, 2, R>& dst, const RightT<mshadow::gpu, 2, R>& src, const double& temperature = 1.0) {
            const int num_threads = mshadow::cuda::kBaseThreadNum;
            const int thread_bits = mshadow::cuda::kBaseThreadBits;

            dim3 tiles(dst.size(0));
            // block size is a matrix column
            dim3 within_tile(num_threads);
            mshadow::cuda::CheckLaunchParam(tiles, within_tile, "Softmax");
            cudaStream_t stream = mshadow::Stream<mshadow::gpu>::GetStream(dst.stream_);

            if (dst.size(1) <= MAX_ROW_SIZE_FOR_CACHED) {
                SoftmaxKernelCached<thread_bits, R>
                        <<<tiles, within_tile, 0, stream>>>
                        (mshadow::expr::MakePlan(dst),
                        mshadow::expr::MakePlan(src),
                        dst.size(1),
                        temperature);
            } else {
                SoftmaxKernel<thread_bits, R>
                        <<<tiles, within_tile, 0, stream>>>
                        (mshadow::expr::MakePlan(dst),
                        mshadow::expr::MakePlan(src),
                        dst.size(1),
                        temperature);
            }
            cudaDeviceSynchronize();
        }

        template<typename R, template<class,int,class>class LeftT, template<class,int,class>class RightT>
        void softmax_colwise(LeftT<mshadow::gpu, 2, R>& dst, const RightT<mshadow::gpu, 2, R>& src, const double& temperature = 1.0) {
            const int num_threads = mshadow::cuda::kBaseThreadNum;
            const int thread_bits = mshadow::cuda::kBaseThreadBits;

            dim3 tiles(dst.size(1));
            // block size is a matrix column
            dim3 within_tile(num_threads);
            mshadow::cuda::CheckLaunchParam(tiles, within_tile, "Softmax");
            cudaStream_t stream = mshadow::Stream<mshadow::gpu>::GetStream(dst.stream_);

            if (dst.size(0) <= MAX_ROW_SIZE_FOR_CACHED) {
                SoftmaxKernelCached<thread_bits, R>
                        <<<tiles, within_tile, 0, stream>>>
                        (mshadow::expr::MakePlan(dst.T()),
                        mshadow::expr::MakePlan(src.T()),
                        dst.size(0),
                        temperature);
            } else {
                SoftmaxKernel<thread_bits, R>
                        <<<tiles, within_tile, 0, stream>>>
                        (mshadow::expr::MakePlan(dst.T()),
                        mshadow::expr::MakePlan(src.T()),
                        dst.size(0),
                        temperature);
            }
            cudaDeviceSynchronize();
        }
    #endif

    template<typename R, typename DstPlan, typename SrcPlan>
    inline void softmax_rowwise(DstPlan dst, SrcPlan src, mshadow::index_t rows, mshadow::index_t cols, const double& temperature) {
        for (mshadow::index_t y = 0; y < rows; ++y) {
            R mmax = src.Eval(y, 0);
            for (mshadow::index_t x = 1; x < cols; ++x) {
                if (mmax < src.Eval(y, x)) mmax = src.Eval(y, x);
            }
            R sum = 0.0f;
            for (mshadow::index_t x = 0; x < cols; ++x) {
                dst.REval(y, x) = std::exp((src.Eval(y, x) - mmax) / temperature);
                sum += dst.Eval(y, x);
            }
            for (mshadow::index_t x = 0; x < cols; ++x) {
                dst.REval(y, x) /= sum;
            }
        }
    }

    template<typename R, template<class,int,class>class LeftT, template<class,int,class>class RightT>
    inline void softmax_rowwise(LeftT<mshadow::cpu, 2, R>& dst, const RightT<mshadow::cpu, 2, R>& src, const double& temperature) {
        softmax_rowwise<R>(mshadow::expr::MakePlan(dst),
                           mshadow::expr::MakePlan(src),
                           dst.size(0),
                           dst.size(1),
                           temperature);

    }

    template<typename R, template<class,int,class>class LeftT, template<class,int,class>class RightT>
    void softmax_colwise(LeftT<mshadow::cpu, 2, R>& dst, const RightT<mshadow::cpu, 2, R>& src, const double& temperature = 1.0) {
        softmax_rowwise<R>(mshadow::expr::MakePlan(dst.T()),
                           mshadow::expr::MakePlan(src.T()),
                           dst.size(1),
                           dst.size(0),
                           temperature);
    }
} // namespace internal

#endif
