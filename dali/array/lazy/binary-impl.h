#include "dali/array/function/args/mshadow_wrapper.h"
#include "dali/array/function/lazy_function.h"
#include "dali/array/TensorFunctions.h"



template<template<class>class Functor, typename LeftT, typename RightT>
struct LazyBinary : public LazyFunction<LazyBinary<Functor,LeftT,RightT>, LeftT, RightT> {
    LeftT  left;
    RightT right;

    LazyBinary(const LeftT& left_, const RightT& right_) :
            LazyFunction<LazyBinary<Functor,LeftT,RightT>, LeftT, RightT>(left_, right_),
            left(left_),
            right(right_) {
    }

    template<int devT,typename T>
    auto to_mshadow_expr(memory::Device device) const ->
            decltype(
                mshadow::expr::F<Functor<T>>(
                     MshadowWrapper<devT,T,decltype(left)>::wrap(left, device),
                     MshadowWrapper<devT,T,decltype(right)>::wrap(right, device)
                )
            ) {
        auto left_expr  = MshadowWrapper<devT,T,decltype(left)>::wrap(left,  device);
        auto right_expr = MshadowWrapper<devT,T,decltype(right)>::wrap(right, device);
        return mshadow::expr::F<Functor<T>>(left_expr, right_expr);
    }
};


namespace lazy {
    template <typename T, typename T2>
    LazyBinary<tensor_ops::op::add, T, T2> add(T a, T2 b) {
        return LazyBinary<tensor_ops::op::add, T, T2>(a, b);
    }

    template <typename T, typename T2>
    LazyBinary<tensor_ops::op::sub, T, T2> sub(T a, T2 b) {
        return LazyBinary<tensor_ops::op::sub, T, T2>(a, b);
    }

    template <typename T, typename T2>
    LazyBinary<tensor_ops::op::eltmul, T, T2> eltmul(T a, T2 b) {
        return LazyBinary<tensor_ops::op::eltmul, T, T2>(a, b);
    }

    template <typename T, typename T2>
    LazyBinary<tensor_ops::op::eltdiv, T, T2> eltdiv(T a, T2 b) {
        return LazyBinary<tensor_ops::op::eltdiv, T, T2>(a, b);
    }
}  // namespace lazy