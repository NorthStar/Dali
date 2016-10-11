#include "reducers.h"
#include "dali/array/array.h"
#include "dali/array/lazy/reducers.h"
#include "dali/array/lazy/cast.h"
#define DALI_USE_LAZY 1
#include "dali/array/op.h"


namespace old_op {
    Assignable<Array> sum(const Array& x) {
        return lazy::sum(x);
    }

    Assignable<Array> product(const Array& x) {
        return lazy::product(x);
    }

    Assignable<Array> mean(const Array& x) {
        if (x.dtype() == DTYPE_INT32) {
            return lazy::sum(lazy::astype<double>(x) / (double)x.number_of_elements());
        }
        return lazy::sum(x / x.number_of_elements());
    }

    Assignable<Array> L2_norm(const Array& x) {
        return Assignable<Array>([x](Array& out, const OPERATOR_T& operator_t) {
            Array temp = lazy::sum(lazy::square(x));
            lazy::Eval<Array>::eval(lazy::sqrt(temp)).assign_to(out, operator_t);
        });
    }

    Assignable<Array> L2_norm(const Array& x, int axis) {
        if (axis < 0) axis = x.ndim() + axis;
        return Assignable<Array>([x, axis](Array& out, const OPERATOR_T& operator_t) {
            Array temp = lazy::sum(lazy::square(x), axis);
            lazy::Eval<Array>::eval(lazy::sqrt(temp)).assign_to(out, operator_t);
        });
    }

    Assignable<Array> min(const Array& x) {
        return lazy::min(x);
    }

    Assignable<Array> max(const Array& x) {
        return lazy::max(x);
    }

    Assignable<Array> sum(const Array& x, int axis) {
        if (axis < 0) axis = x.ndim() + axis;
        return lazy::sum(x, axis);
    }

    Assignable<Array> product(const Array& x, int axis) {
        if (axis < 0) axis = x.ndim() + axis;
        return lazy::product(x, axis);
    }

    Assignable<Array> min(const Array& x, int axis) {
        if (axis < 0) axis = x.ndim() + axis;
        return lazy::min(x, axis);
    }

    Assignable<Array> max(const Array& x, int axis) {
        if (axis < 0) axis = x.ndim() + axis;
        return lazy::max(x, axis);
    }

    Assignable<Array> argmin(const Array& x) {
        return lazy::argmin(x.ravel(), 0);
    }

    Assignable<Array> argmax(const Array& x) {
        return lazy::argmax(x.ravel(), 0);
    }

    Assignable<Array> argmin(const Array& x, int axis) {
        if (axis < 0) axis = x.ndim() + axis;
        return lazy::argmin(x, axis);
    }

    Assignable<Array> argmax(const Array& x, int axis) {
        if (axis < 0) axis = x.ndim() + axis;
        return lazy::argmax(x, axis);
    }

    Assignable<Array> mean(const Array& x, int axis) {
        if (axis < 0) axis = x.ndim() + axis;
        return Assignable<Array>([x, axis](Array& out, const OPERATOR_T& operator_t) {
            if (x.dtype() == DTYPE_INT32) {
                auto reduced = lazy::sum(lazy::astype<double>(x), axis);
                lazy::Eval<Array>::eval(reduced / x.shape()[axis]).assign_to(out, operator_t);
            } else {
                auto reduced = lazy::sum(x, axis);
                lazy::Eval<Array>::eval(reduced / x.shape()[axis]/*size of reduced axis*/).assign_to(out, operator_t);
            }
        });
    }
}; // namespace op
