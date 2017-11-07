// #include "dali/array/lazy/reducers.h"

// namespace internal {
//     enum REDUCTION_INSTR_T {
//         REDUCTION_INSTR_T_ALL       = 0,
//         REDUCTION_INSTR_T_CONTIG    = 1,
//         REDUCTION_INSTR_T_NONCONTIG = 2,
//         REDUCTION_INSTR_T_NONE      = 3
//     };

//     struct ReductionInstruction {
//         REDUCTION_INSTR_T type = REDUCTION_INSTR_T_NONE;

//         int reduce_start = -1;
//         int reduce_end   = -1;
//         std::vector<int> noncontiguous_axes;

//         inline bool all() const           { return type == REDUCTION_INSTR_T_ALL;       }
//         inline bool contiguous() const    { return type == REDUCTION_INSTR_T_CONTIG;    }
//         inline bool noncontiguous() const { return type == REDUCTION_INSTR_T_NONCONTIG; }
//         inline bool none() const          { return type == REDUCTION_INSTR_T_NONE;      }
//     };


//     // deduce how many reductions need to be made, and if an all-reduce could achieve it
//     ReductionInstruction requires_reduction(const Array& output, const std::vector<int>& in_bshape);
// }

// namespace lazy {

//     template<typename OutType>
//     template<typename Class, typename... Args>
//     inline Assignable<OutType> Eval<OutType>::eval_no_autoreduce(const LazyFunction<Class, Args...>& expr) {
//         return LazyEvaluator<OutType, Class>::run(expr.self());
//     }

//     template<typename OutType>
//     template<typename Class, typename... Args>
//     inline Assignable<OutType> Eval<OutType>::eval(const LazyFunction<Class, Args...>& expr) {
//         auto this_self   = expr.self();
//         auto this_bshape = expr.bshape();

//         return Assignable<OutType>([this_self, this_bshape](OutType& out, const OPERATOR_T& operator_t) {
//             internal::ReductionInstruction reduction_instruction;
//             if (operator_t == OPERATOR_T_LSE) {
//                 reduction_instruction = internal::requires_reduction(out, this_bshape);
//             }
//             ASSERT2(reduction_instruction.none(),
//                     "Autoreduction for <<= can only be performed on Arrays.");
//             LazyEvaluator<OutType, Class>::run(this_self).assign_to(out, operator_t);
//         });
//     }

//     // Here we specialize the lazy evaluation so that the resulting uncomputed expression
//     // can not be further reduced (which would lead to infinite recursion => e.g. sum(sum(sum(a))) etc...)
//     template<typename OutType>
//     template<typename Functor, typename ExprT, typename... Args>
//     inline Assignable<OutType> Eval<OutType>::eval(const LazyFunction<LazyAllReducer<Functor, ExprT>, Args...>& expr) {
//         return eval_no_autoreduce(expr);
//     }

//     template<>
//     struct Eval<Array> {

//         template<typename Class, typename... Args>
//         static inline Assignable<Array> eval_no_autoreduce(const LazyFunction<Class, Args...>& expr) {
//             return LazyEvaluator<Array, Class>::run(expr.self());
//         }

//         template<typename Class, typename... Args>
//         static inline Assignable<Array> eval(const LazyFunction<Class, Args...>& expr) {
//             auto this_self   = expr.self();
//             auto this_bshape = expr.bshape();

//             return Assignable<Array>([this_self, this_bshape](Array& out, const OPERATOR_T& operator_t) {
//                 internal::ReductionInstruction reduction_instruction;
//                 if (operator_t == OPERATOR_T_LSE) {
//                     reduction_instruction = internal::requires_reduction(out, this_bshape);
//                 }
//                 if (reduction_instruction.contiguous()) {
//                     auto reduced_expr = lazy::sum(
//                            this_self,
//                            /*reduce_start=*/reduction_instruction.reduce_start,
//                            /*reduce_end=*/reduction_instruction.reduce_end,
//                            /*keepdims=*/true);
//                     auto computation_with_reduce = lazy::Eval<Array>::eval_no_autoreduce(reduced_expr);
//                     computation_with_reduce.assign_to(out, operator_t);
//                 } else if (reduction_instruction.noncontiguous()) {
//                     ASSERT2(false, "Noncontiguous axis reduction is not supported yet.");
//                 } else if (reduction_instruction.all()) {
//                     auto reduced_expr = lazy::sum(this_self);
//                     auto computation_with_reduce = lazy::Eval<Array>::eval_no_autoreduce(reduced_expr);
//                     auto out_as_scalar = out.copyless_reshape({});
//                     computation_with_reduce.assign_to(out_as_scalar, operator_t);
//                 } else {
//                     LazyEvaluator<Array, Class>::run(this_self).assign_to(out, operator_t);
//                 }
//             });
//         }

//         // Here we specialize the lazy evaluation so that the resulting uncomputed expression
//         // can not be further reduced (which would lead to infinite recursion => e.g. sum(sum(sum(a))) etc...)
//         template<typename Functor, typename ExprT, typename... Args>
//         static inline Assignable<Array> eval(const LazyFunction<LazyAllReducer<Functor, ExprT>, Args...>& expr) {
//             return eval_no_autoreduce(expr);
//         }

//     };

//     // Eval with operator (only instantiate a single operator)
//     template<OPERATOR_T intented_operator_t, typename OutType>
//     struct EvalWithOperator {
//         template<typename Class, typename... Args>
//         static inline Assignable<OutType> eval_no_autoreduce(const LazyFunction<Class, Args...>& expr) {
//             return LazyEvaluator<OutType, Class>::template run_with_operator<intented_operator_t>(expr.self());
//         }

//         template<typename Class, typename... Args>
//         static inline Assignable<OutType> eval(const LazyFunction<Class, Args...>& expr) {
//             auto this_self   = expr.self();
//             auto this_bshape = expr.bshape();

//             return Assignable<OutType>([this_self, this_bshape](OutType& out, const OPERATOR_T& operator_t) {
//                 ASSERT2(operator_t == intented_operator_t,
//                     utils::MS() << "Assignable constructed for operator "
//                                 << operator_to_name(intented_operator_t)
//                                 << " but got " << operator_to_name(operator_t)
//                                 << " instead");
//                 LazyEvaluator<OutType, Class>::template run_with_operator<intented_operator_t>(
//                     this_self
//                 ).assign_to(
//                     out,
//                     intented_operator_t
//                 );
//             });
//         }

//         template<typename Functor, typename ExprT, typename... Args>
//         static inline Assignable<OutType> eval(const LazyFunction<LazyAllReducer<Functor, ExprT>, Args...>& expr) {
//             return EvalWithOperator<intented_operator_t,OutType>::eval_no_autoreduce(expr);
//         }
//     };

//     template<typename OutType>
//     struct EvalWithOperator<OPERATOR_T_LSE, OutType> {
//         template<typename Class, typename... Args>
//         static inline Assignable<OutType> eval(const LazyFunction<Class, Args...>& expr) {
//             auto this_self   = expr.self();
//             auto this_bshape = expr.bshape();

//             return Assignable<OutType>([this_self, this_bshape](OutType& out, const OPERATOR_T& operator_t) {
//                 ASSERT2(operator_t == OPERATOR_T_LSE,
//                     utils::MS() << "Assignable constructed for operator "
//                                 << operator_to_name(OPERATOR_T_LSE)
//                                 << " but got " << operator_to_name(operator_t)
//                                 << " instead");
//                 auto reduction_instruction = internal::requires_reduction(
//                     out,
//                     this_bshape
//                 );
//                 if (reduction_instruction.contiguous()) {
//                     auto reduced_expr = lazy::sum(
//                            this_self,
//                            /*reduce_start=*/reduction_instruction.reduce_start,
//                            /*reduce_end=*/reduction_instruction.reduce_end,
//                            /*keepdims=*/true);
//                     auto computation_with_reduce = EvalWithOperator<OPERATOR_T_LSE,OutType>::eval_no_autoreduce(
//                         reduced_expr
//                     );
//                     computation_with_reduce.assign_to(
//                         out,
//                         OPERATOR_T_LSE
//                     );
//                 } else if (reduction_instruction.all()) {
//                     auto reduced_expr = lazy::sum(this_self);
//                     auto computation_with_reduce = EvalWithOperator<OPERATOR_T_LSE,OutType>::eval_no_autoreduce(
//                         reduced_expr
//                     );
//                     auto out_as_scalar = out.copyless_reshape({});
//                     computation_with_reduce.assign_to(
//                         out_as_scalar,
//                         OPERATOR_T_LSE
//                     );
//                 } else if (reduction_instruction.noncontiguous()) {
//                     ASSERT2(false, "Noncontiguous axis reduction is not supported yet.");
//                 } else {
//                     LazyEvaluator<OutType, Class>::template run_with_operator<OPERATOR_T_LSE>(
//                         this_self
//                     ).assign_to(
//                         out,
//                         OPERATOR_T_LSE
//                     );
//                 }
//             });
//         }

//         template<typename Class, typename... Args>
//         static inline Assignable<OutType> eval_no_autoreduce(const LazyFunction<Class, Args...>& expr) {
//             return LazyEvaluator<OutType, Class>::template run_with_operator<OPERATOR_T_LSE>(expr.self());
//         }

//         template<typename Functor, typename ExprT, typename... Args>
//         static inline Assignable<OutType> eval(const LazyFunction<LazyAllReducer<Functor, ExprT>, Args...>& expr) {
//             return EvalWithOperator<OPERATOR_T_LSE, OutType>::eval_no_autoreduce(expr);
//         }
//     };

// }  // namespace lazy
