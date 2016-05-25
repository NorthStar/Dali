#include <gtest/gtest.h>

#include "dali/array/array.h"
#include "dali/array/dtype.h"

template<typename T>
void EXPECT_EQ_DTYPE(const T& reference, const Array& result, const DType& dtype) {
    if (dtype == DTYPE_FLOAT) {
        EXPECT_EQ((float)reference, (float)result);
    } else if (dtype == DTYPE_INT32) {
        EXPECT_EQ((int)reference, (int)result);
    } else if (dtype == DTYPE_DOUBLE) {
        EXPECT_EQ((double)reference, (double)result);
    }
}

template<typename T>
void EXPECT_NEAR_DTYPE(const T& reference, const Array& result, float eps, const DType& dtype) {
    if (dtype == DTYPE_FLOAT) {
        EXPECT_NEAR((float)reference, (float)result, (float)eps);
    } else if (dtype == DTYPE_INT32) {
        EXPECT_NEAR((int)reference, (int)result, eps);
    } else if (dtype == DTYPE_DOUBLE) {
        EXPECT_NEAR((double)reference, (double)result, (double)eps);
    }
}
