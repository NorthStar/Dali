#include "reducers.h"
#include "dali/tensor/tape.h"
#include "dali/array/op2/binary.h"
#include "dali/array/op2/unary.h"
#include "dali/array/op2/reducers.h"
#include "dali/array/op/other.h"
#include "dali/array/op_overload/common.h"
#include "dali/array/op_overload/nonlazy.h"
#include "dali/tensor/tensor_macros.h"
#include "dali/utils/print_utils.h"

namespace tensor_ops {
    Tensor sum(const Tensor& tensor) {
        if (tensor.number_of_elements() == 1) {
            auto out = tensor;
            out.w = tensor.w.reshape({});
            out.dw = tensor.dw.reshape({});
            return out;
        } else {
            // TODO(jonathan, szymon) also makes sure that device
            // of input tensor is also used here

            Tensor out(tensor.w.sum());

            if (graph::backprop_enabled() && !tensor.constant) {
                auto out_dw = out.dw;
                auto tensor_dw = tensor.dw;
                graph::emplace_back([tensor_dw, out_dw]() mutable {
                    tensor_dw <<= out_dw.broadcast_scalar_to_ndim(tensor_dw.ndim());
                });
            }
            return out;
        }
    }

    Tensor mean(const Tensor& tensor) {
        if (tensor.number_of_elements() == 1) {
            auto out = tensor;
            out.w = tensor.w.reshape({});
            out.dw = tensor.dw.reshape({});
            return out;
        } else {
            Tensor out(tensor.w.mean());
            if (graph::backprop_enabled() && !tensor.constant) {
                auto out_dw = out.dw;
                auto tensor_dw = tensor.dw;
                graph::emplace_back([tensor_dw, out_dw]() mutable {
                    tensor_dw <<= (
                        out_dw.broadcast_scalar_to_ndim(tensor_dw.ndim()) /
                        tensor_dw.number_of_elements()
                    );
                });
            }
            return out;
        }
    }

    Tensor L2_norm(const Tensor& tensor) {
        Tensor out(tensor.w.L2_norm());
        if (graph::backprop_enabled() && !tensor.constant)
            graph::emplace_back([tensor, out]() mutable {
                MAYBE_GRAD(tensor) <<= (
                    tensor.w * (
                        out.dw.broadcast_scalar_to_ndim(tensor.ndim()) /
                        out.w.broadcast_scalar_to_ndim(tensor.ndim())
                    )
                );
            });
        return out;
    }

    Tensor L2_norm(const Tensor& tensor, int axis) {
        if (axis < 0) axis = axis + tensor.ndim();
        Tensor out(tensor.w.L2_norm(axis));
        if (graph::backprop_enabled() && !tensor.constant)
            graph::emplace_back([tensor, out, axis]() mutable {
                MAYBE_GRAD(tensor) <<= (
                    tensor.w * (
                        out.dw.insert_broadcast_axis(axis) /
                        out.w.insert_broadcast_axis(axis)
                    )
                );
            });
        return out;
    }

    Tensor sum(const Tensor& tensor, int axis) {
        if (axis < 0) axis = axis + tensor.ndim();
        Tensor out(op::sum(tensor.w, {axis}));
        if (graph::backprop_enabled() && !tensor.constant) {
            auto tensor_dw = tensor.dw;
            auto out_dw = out.dw;
            graph::emplace_back([tensor_dw, out_dw, axis]() mutable {
                // make sure output has same shape as input
                // with the reduced dimension returned as
                // broadcasted
                auto reshaped_gradient = out_dw.insert_broadcast_axis(axis);
                tensor_dw <<= reshaped_gradient;
            });
        }
        return out;
    }

    Tensor mean(const Tensor& tensor, int axis) {
        if (axis < 0) axis = axis + tensor.ndim();
        Tensor out(op::mean(tensor.w, {axis}));
        if (graph::backprop_enabled() && !tensor.constant) {
            auto tensor_dw = tensor.dw;
            auto out_dw = out.dw;
            graph::emplace_back([tensor_dw, out_dw, axis]() mutable {
                if (axis < 0) axis = axis + tensor_dw.ndim();
                int axis_size = tensor_dw.shape()[axis];
                auto reshaped_gradient = out_dw.insert_broadcast_axis(axis);
                tensor_dw <<= reshaped_gradient / axis_size;
            });
        }
        return out;
    }


    #define DALI_TENSOR_SUBSAMPLE_ALL_REDUCTION(FUNCTION_NAME)\
        Tensor FUNCTION_NAME(const Tensor& tensor) {\
            if (tensor.number_of_elements() == 1) {\
                auto out = tensor;\
                out.w = tensor.w.reshape({});\
                out.dw = tensor.dw.reshape({});\
                return out;\
            } else {\
                Tensor out(tensor.w.FUNCTION_NAME());\
                if (graph::backprop_enabled() && !tensor.constant)\
                    graph::emplace_back([tensor, out]() mutable {\
                        tensor.dw <<= op::equals(\
                            out.w.broadcast_scalar_to_ndim(tensor.ndim()),\
                            tensor.w\
                        ) * out.dw.broadcast_scalar_to_ndim(tensor.ndim());\
                    });\
                return out;\
            }\
        }\

    DALI_TENSOR_SUBSAMPLE_ALL_REDUCTION(min);
    DALI_TENSOR_SUBSAMPLE_ALL_REDUCTION(max);

    #define DALI_TENSOR_SUBSAMPLE_AXIS_REDUCTION(FUNCTION_NAME, OPNAME)\
        Tensor FUNCTION_NAME(const Tensor& tensor, int axis) {\
            if (axis < 0) axis = axis + tensor.ndim();\
            Tensor out(OPNAME(tensor.w, {axis}));\
            if (graph::backprop_enabled() && !tensor.constant)\
                graph::emplace_back([tensor, out, axis]() mutable {\
                    tensor.dw <<= op::equals(\
                            out.w.insert_broadcast_axis(axis),\
                            tensor.w\
                        ) * out.dw.insert_broadcast_axis(axis);\
                });\
            return out;\
        }\

    DALI_TENSOR_SUBSAMPLE_AXIS_REDUCTION(min, op::min);
    DALI_TENSOR_SUBSAMPLE_AXIS_REDUCTION(max, op::max);


    #define DALI_TENSOR_SUBSAMPLE_ALL_REDUCTION(FUNCTION_NAME)\
        Tensor FUNCTION_NAME(const Tensor& tensor) {\
            if (tensor.number_of_elements() == 1) {\
                auto out = tensor;\
                out.w = tensor.w.reshape({});\
                out.dw = tensor.dw.reshape({});\
                return out;\
            } else {\
                Tensor out(tensor.w.FUNCTION_NAME());\
                if (graph::backprop_enabled() && !tensor.constant)\
                    graph::emplace_back([tensor, out]() mutable {\
                        tensor.dw <<= op::equals(\
                            out.w.broadcast_scalar_to_ndim(tensor.ndim()),\
                            tensor.w\
                        ) * out.dw.broadcast_scalar_to_ndim(tensor.ndim());\
                    });\
                return out;\
            }\
        }\

    #define DALI_TENSOR_GETINDICES_ALL_REDUCTION(FUNCTION_NAME)\
        Tensor FUNCTION_NAME(const Tensor& tensor) {\
            return Tensor(op::FUNCTION_NAME(tensor.w));\
        }\

    DALI_TENSOR_GETINDICES_ALL_REDUCTION(argmin);
    DALI_TENSOR_GETINDICES_ALL_REDUCTION(argmax);

    Tensor argsort(const Tensor& tensor) {
        return Tensor(op::argsort(tensor.w.ravel(), 0));
    }

    #define DALI_TENSOR_GETINDICES_AXIS_REDUCTION(FUNCTION_NAME)\
        Tensor FUNCTION_NAME(const Tensor& tensor, int axis) {\
            return Tensor(op::FUNCTION_NAME(tensor.w, axis));\
        }\

    DALI_TENSOR_GETINDICES_AXIS_REDUCTION(argmin);
    DALI_TENSOR_GETINDICES_AXIS_REDUCTION(argmax);
    DALI_TENSOR_GETINDICES_AXIS_REDUCTION(argsort);
}
