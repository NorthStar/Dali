#ifndef DALI_ARRAY_OP2_ELEMENTWISE_OPERATION_H
#define DALI_ARRAY_OP2_ELEMENTWISE_OPERATION_H

#include "dali/array/dtype.h"
#include "dali/array/op2/expression/expression.h"
#include <string>

namespace op {
    // elementwise kernel given by name. assumes
    // return type is unchanged from a's
    Expression elementwise(
        const Expression& a,
        const std::string& functor_name
    );

    // pair-wise kernel. Will type promote arguments
    // so that they have the same type when
    // given to the functor:
    // - float w/. double => double
    // - float w/. int => float
    // - double w/. int => double
    Expression elementwise(
        const Expression& a,
        const Expression& b,
        const std::string& functor_name
    );

    // call a kernel on a pair of arguments. Assumes
    // both arguments should be of the same type. Peforms
    // type promotion on the arguments if not. Will paste
    // and run the associated code `kernel_code` during
    // compilation and usage. (Warning: this might cause
    // collisions when a name is used multiple times)
    Expression binary_kernel_function(
        const Expression& a,
        const Expression& b,
        const std::string& function_name,
        const std::string& kernel_code
    );

    // Perform a type conversion by casting the values in x
    // to another dtype. Use rounding when casting to integers
    // for more predictable results
    Expression astype(const Expression& x, DType dtype);
    // static_cast one type to another. This can cause unpredictable
    // behavior on floats->integers based on underlying
    // hardware/implementation
    Expression unsafe_cast(const Expression& x, DType dtype);
    // Perform rounding on a value to nearest integer.
    // Note: equivalent to floor(x + 0.5).
    Expression round(const Expression& x);

    // type-promote arguments if necessary and check whether their
    // ranks are compatible (equal or one is a scalar)
    std::tuple<Expression, Expression> ensure_arguments_compatible(
        const Expression& a, const Expression& b
    );
} // namespace op2

#endif  // DALI_ARRAY_OP2_ELEMENTWISE_OPERATION_H
