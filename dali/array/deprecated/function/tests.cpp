#include <gtest/gtest.h>

#include "dali/utils/print_utils.h"

#include "dali/runtime_config.h"
#include "dali/array/function/function.h"
#define DALI_USE_LAZY 0
#include "dali/array/op.h"

using namespace op;


TEST(FunctionTests, lazy_lse) {
    Array s1 = Array::ones({3,4}, DTYPE_INT32);
    Array s2 = Array::ones({3,4}, DTYPE_INT32);

    Array target = Array::zeros({3,4}, DTYPE_INT32);

    target <<= s1 + s2;

    for (int i = 0; i < target.number_of_elements(); ++i) {
        EXPECT_EQ(2, (int)target(i));
    }
}


TEST(FunctionTests, lse_3D) {
    Array target = Array({3,2,2});
    Array source = Array({3,2,2});

    target <<= source;
}

TEST(FunctionTests, lse) {
    Array target = Array::zeros({3,4}, DTYPE_INT32);
    Array source = Array::arange({3,4}, DTYPE_INT32);

    target <<= source;

    for (int i = 0; i < target.number_of_elements(); ++i) {
        EXPECT_EQ(i, (int)target(i));
    }
}

TEST(FunctionTests, broadcasted_lse) {
    Array target = Array::zeros({3}, DTYPE_INT32)[Slice(0,3)][Broadcast()];
    Array source = Array::ones({3,4}, DTYPE_INT32);

    target <<= source;

    for (int i = 0; i < target.number_of_elements(); ++i) {
        EXPECT_EQ(4,  (int)target(i));
    }
}

TEST(FunctionTests, broadcasted_lse2) {
    Array target = Array::zeros({4}, DTYPE_INT32)[Broadcast()][Slice(0,4)];
    Array source = Array::ones({3,4}, DTYPE_INT32);

    target <<= source;

    for (int i = 0; i < target.number_of_elements(); ++i) {
        EXPECT_EQ(3,  (int)target(i));
    }
}

TEST(FunctionTests, blas_friendly_tensor) {
    Array s1 = Array::ones({3,4}, DTYPE_INT32);
    Array s2 = Array::ones({1,4}, DTYPE_INT32);
    Array s3 = Array::ones({3,1}, DTYPE_INT32);
    Array s4 = Array::ones({1,1}, DTYPE_INT32);

    auto verify_result = [](std::string testname, Array arr, bool expected_tranpose, int expected_stride) {
        SCOPED_TRACE(testname);
        TypedArray<memory::DEVICE_T_CPU,int> typed_arr(arr, memory::Device::cpu(), arr.shape());
        bool transpose;
        mshadow::Tensor<mshadow::cpu, 2, int> mtensor;
        std::tie(transpose, mtensor) = typed_arr.blas_friendly_tensor();
        EXPECT_EQ(expected_tranpose, transpose);
        EXPECT_EQ(expected_stride, mtensor.stride_);
    };


    verify_result("3x4, not transposed", s1,             false, 4);
    verify_result("1x4, not transposed", s2,             false, 4);
    verify_result("3x1, not transposed", s3,             false, 1);
    verify_result("1x1, not transposed", s4,             false, 1);

    verify_result("3x4, transposed",     s1.transpose(), true,  4);
    verify_result("1x4, transposed",     s2.transpose(), true,  4);
    verify_result("3x1, transposed",     s3.transpose(), true,  1);
    verify_result("1x1, transposed",     s4.transpose(), false, 1);
}
