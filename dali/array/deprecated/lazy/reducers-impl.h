// #include <vector>

// #include "dali/array/shape.h"
// #include "dali/array/function/lazy_function.h"
// #include "dali/array/lazy/base_lazy_axis_reducer.h"

// #include <mshadow/extension/reduce_with_axis.h>

// template<class Functor, typename ExprT>
// struct LazyAllReducer : public LazyFunction<LazyAllReducer<Functor,ExprT>, ExprT> {
//     static const int evaluation_dim;
//     ExprT expr;

//     static std::vector<int> lazy_output_bshape(const ExprT&) {
//         return {};
//     }

//     LazyAllReducer(const ExprT& expr_) :
//             LazyFunction<LazyAllReducer<Functor,ExprT>, ExprT>(expr_),
//             expr(expr_) {
//     }

//     template<int devT, typename T, int ndim>
//     auto to_mshadow_expr(memory::Device device, const std::vector<int>& output_shape, const lazy::EvaluationSpec<devT, T, ndim>& wrap_array) const ->
//             decltype(
//                 mshadow::expr::reduce_all<Functor>(
//                     MshadowWrapper<devT,T,decltype(expr)>::wrap(
//                         expr, device, output_shape,
//                         wrap_array.template d<lazy::OptimalNdimForInput<ExprT, ndim>::value>()
//                     )
//                 )
//             ) {

//         auto left_expr  =
//                 MshadowWrapper<devT,T,decltype(expr)>::wrap(
//                         expr, device, bshape2shape(expr.bshape()), wrap_array.template d<lazy::OptimalNdimForInput<ExprT, ndim>::value>()
//                 );
//         return mshadow::expr::reduce_all<Functor>(left_expr);
//     }
// };

// template<class Functor, typename ExprT, bool return_indices>
// struct LazyAxisReducer {
// };

// template<class Functor, typename ExprT>
// const int LazyAllReducer<Functor, ExprT>::evaluation_dim = 1;

// template<class Functor, typename ExprT>
// struct LazyFunctionName<LazyAllReducer<Functor, ExprT>> {
//     static std::string name;
// };
// template<class Functor, typename ExprT>
// std::string LazyFunctionName<LazyAllReducer<Functor, ExprT>>::name = "lazy_reduce_all";

// template<class Functor, typename ExprT>
// struct LazyAxisReducer<Functor, ExprT, false> : public BaseLazyAxisReducer<LazyAxisReducer<Functor, ExprT, false>, ExprT, Functor, false> {
//     using BaseLazyAxisReducer<LazyAxisReducer<Functor, ExprT, false>, ExprT, Functor, false>::BaseLazyAxisReducer; // inherit parent constructor
// };

// template<class Functor, typename ExprT>
// struct LazyFunctionName<LazyAxisReducer<Functor, ExprT, false>> {
//     static std::string name;
// };
// template<class Functor, typename ExprT>
// std::string LazyFunctionName<LazyAxisReducer<Functor, ExprT, false>>::name = "lazy_reduce_axis";

// template<class Functor, typename ExprT>
// struct LazyAxisReducer<Functor, ExprT, true> : public BaseLazyAxisReducer<LazyAxisReducer<Functor, ExprT, true>, ExprT, Functor, true> {
//     using BaseLazyAxisReducer<LazyAxisReducer<Functor, ExprT, true>, ExprT, Functor, true>::BaseLazyAxisReducer; // inherit parent constructor
// };

// template<class Functor, typename ExprT>
// struct LazyFunctionName<LazyAxisReducer<Functor, ExprT, true>> {
//     static std::string name;
// };
// template<class Functor, typename ExprT>
// std::string LazyFunctionName<LazyAxisReducer<Functor, ExprT, true>>::name = "lazy_arg_reduce_axis";

// namespace lazy {
//     #define DALI_LAZY_ARRAY_ALL_REDUCER(OPNAME, REDUCERNAME)\
//         template<typename ExprT>\
//         LazyAllReducer<REDUCERNAME, ExprT> OPNAME(const Exp<ExprT>& expr) {\
//             return LazyAllReducer<REDUCERNAME, ExprT>(expr.self());\
//         }\

//     DALI_LAZY_ARRAY_ALL_REDUCER(sum, mshadow::red::sum);
//     DALI_LAZY_ARRAY_ALL_REDUCER(product, mshadow::red::product);
//     DALI_LAZY_ARRAY_ALL_REDUCER(min, mshadow::red::minimum);
//     DALI_LAZY_ARRAY_ALL_REDUCER(max, mshadow::red::maximum);

//     #define DALI_DEFINE_LAZY_ARRAY_AXIS_REDUCER(OPNAME, REDUCERNAME, RETURN_INDICES)\
//         template<typename ExprT>\
//         LazyAxisReducer<REDUCERNAME, ExprT, RETURN_INDICES> OPNAME(const Exp<ExprT>& expr,\
//                                                                    const int& axis, \
//                                                                    bool keepdims) {\
//             return LazyAxisReducer<REDUCERNAME, ExprT, RETURN_INDICES>(expr.self(), axis, axis + 1, keepdims);\
//         }\
//         template<typename ExprT>\
//         LazyAxisReducer<REDUCERNAME, ExprT, RETURN_INDICES> OPNAME(const Exp<ExprT>& expr, \
//                                                                    const int& reduce_start, \
//                                                                    const int& reduce_end, \
//                                                                    bool keepdims) {\
//             return LazyAxisReducer<REDUCERNAME, ExprT, RETURN_INDICES>(expr.self(), reduce_start, reduce_end, keepdims);\
//         }\

//     DALI_DEFINE_LAZY_ARRAY_AXIS_REDUCER(sum, mshadow::red::sum, false);
//     DALI_DEFINE_LAZY_ARRAY_AXIS_REDUCER(product, mshadow::red::product, false);
//     DALI_DEFINE_LAZY_ARRAY_AXIS_REDUCER(min, mshadow::red::minimum, false);
//     DALI_DEFINE_LAZY_ARRAY_AXIS_REDUCER(max, mshadow::red::maximum, false);

//     DALI_DEFINE_LAZY_ARRAY_AXIS_REDUCER(argmax, mshadow::red::maximum, true);
//     DALI_DEFINE_LAZY_ARRAY_AXIS_REDUCER(argmin, mshadow::red::minimum, true);
// }  // namespace lazy
