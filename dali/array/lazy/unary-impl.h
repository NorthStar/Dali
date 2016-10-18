#include "dali/array/function/lazy_function.h"
#include "dali/array/functor.h"

template<template<class>class Functor, typename ExprT>
struct LazyUnary : public LazyFunction<LazyUnary<Functor,ExprT>, ExprT> {
    ExprT expr;
    static const int evaluation_dim;

    LazyUnary(ExprT expr_) : LazyFunction<LazyUnary<Functor,ExprT>, ExprT>(expr_),
                             expr(expr_) {
    }

    template<int devT, typename T, int ndim>
    auto to_mshadow_expr(memory::Device device, const std::vector<int>& output_shape, const lazy::EvaluationSpec<devT, T, ndim>& wrap_array) const ->
            decltype(
                mshadow::expr::F<Functor<typename functor_helper::UnaryExtractDType<
                    decltype(MshadowWrapper<devT,T,ExprT>::wrap(expr, device, output_shape, wrap_array))>::value>
                >(MshadowWrapper<devT,T,ExprT>::wrap(expr, device, output_shape, wrap_array))
            ) {
        auto left_expr = MshadowWrapper<devT,T,ExprT>::wrap(expr, device, output_shape, wrap_array);
        typedef typename functor_helper::UnaryExtractDType<decltype(left_expr)>::value functor_dtype_t;
        return mshadow::expr::F<Functor<functor_dtype_t>>(left_expr);
    }
};

template<template<class>class Functor, typename ExprT>
const int LazyUnary<Functor, ExprT>::evaluation_dim = lazy::LazyEvaluationDim<ExprT>::value;

template<template<class>class Functor, typename ExprT>
struct LazyFunctionName<LazyUnary<Functor, ExprT>> {
    static std::string name;
};

template<template<class>class Functor, typename ExprT>
std::string LazyFunctionName<LazyUnary<Functor, ExprT>>::name = "UnaryFunctor";

template<template<class>class Functor, typename ExprT>
struct LazyUnaryIndexed : public LazyFunction<LazyUnaryIndexed<Functor,ExprT>, ExprT> {
    ExprT expr;
    static const int evaluation_dim;

    LazyUnaryIndexed(ExprT expr_) : LazyFunction<LazyUnaryIndexed<Functor,ExprT>, ExprT>(expr_),
                             expr(expr_) {
    }

    template<int devT, typename T, int ndim>
    auto to_mshadow_expr(memory::Device device, const std::vector<int>& output_shape, const lazy::EvaluationSpec<devT, T, ndim>& wrap_array) const ->
            decltype(
                mshadow::expr::FIndexed<Functor<typename functor_helper::UnaryExtractDType<
                    decltype(MshadowWrapper<devT,T,ExprT>::wrap(expr, device, output_shape, wrap_array))>::value>
                >(MshadowWrapper<devT,T,ExprT>::wrap(expr, device, output_shape, wrap_array))
            ) {
        auto left_expr = MshadowWrapper<devT,T,ExprT>::wrap(expr, device, output_shape, wrap_array);
        typedef typename functor_helper::UnaryExtractDType<decltype(left_expr)>::value functor_dtype_t;
        return mshadow::expr::FIndexed<Functor<functor_dtype_t>>(left_expr);
    }
};

template<template<class>class Functor, typename ExprT>
const int LazyUnaryIndexed<Functor, ExprT>::evaluation_dim = lazy::LazyEvaluationDim<ExprT>::value;

template<template<class>class Functor, typename ExprT>
struct LazyFunctionName<LazyUnaryIndexed<Functor, ExprT>> {
    static std::string name;
};


template<template<class>class Functor, typename ExprT>
std::string LazyFunctionName<LazyUnaryIndexed<Functor, ExprT>>::name = "UnaryFunctorIndexed";

namespace lazy {
    template<typename ExprT>
    LazyUnary<functor::identity,ExprT> identity(const Exp<ExprT>& expr) {
        return LazyUnary<functor::identity,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::sigmoid,ExprT> sigmoid(const Exp<ExprT>& expr) {
        return LazyUnary<functor::sigmoid,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::tanh,ExprT> tanh(const Exp<ExprT>& expr) {
        return LazyUnary<functor::tanh,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::inv,ExprT> eltinv(const Exp<ExprT>& expr) {
        return LazyUnary<functor::inv,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::exp,ExprT> exp(const Exp<ExprT>& expr) {
        return LazyUnary<functor::exp,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::softplus,ExprT> softplus(const Exp<ExprT>& expr) {
        return LazyUnary<functor::softplus,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::relu,ExprT> relu(const Exp<ExprT>& expr) {
        return LazyUnary<functor::relu,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::log,ExprT> log(const Exp<ExprT>& expr) {
        return LazyUnary<functor::log,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::negative_log,ExprT> negative_log(const Exp<ExprT>& expr) {
        return LazyUnary<functor::negative_log, ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::log_or_zero,ExprT> log_or_zero(const Exp<ExprT>& expr) {
        return LazyUnary<functor::log_or_zero,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::abs,ExprT> abs(const Exp<ExprT>& expr) {
        return LazyUnary<functor::abs,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::sign,ExprT> sign(const Exp<ExprT>& expr) {
        return LazyUnary<functor::sign,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::square,ExprT> square(const Exp<ExprT>& expr) {
        return LazyUnary<functor::square,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::isinfinity,ExprT> isinf(const Exp<ExprT>& expr) {
        return LazyUnary<functor::isinfinity,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::isnotanumber,ExprT> isnan(const Exp<ExprT>& expr) {
        return LazyUnary<functor::isnotanumber,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::cube,ExprT> cube(const Exp<ExprT>& expr) {
        return LazyUnary<functor::cube,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::sqrt_f,ExprT> sqrt(const Exp<ExprT>& expr) {
        return LazyUnary<functor::sqrt_f,ExprT>(expr.self());
    }

    template<typename ExprT>
    LazyUnary<functor::rsqrt,ExprT> rsqrt(const Exp<ExprT>& expr) {
        return LazyUnary<functor::rsqrt,ExprT>(expr.self());
    }

}  // namespace lazy
