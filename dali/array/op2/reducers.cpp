#include "reducers.h"
#include "dali/array/op2/operation.h"
#include "dali/array/op2/binary.h"
#include "dali/array/op2/unary.h"
#include "dali/utils/hash_utils.h"
#include "dali/utils/make_message.h"
#include "dali/array/op2/all_reduce_kernel_utils.h"


///////////////////////////////////////////////////////////////////////////////
//                    HEADERS                                                //
///////////////////////////////////////////////////////////////////////////////

struct ReducerOperationState : public OperationState {
    const operation_state_ptr argument_;
    const std::string functor_name_;

    virtual hash_t optype_hash() const = 0;

    ReducerOperationState(const std::string& functor_name, operation_state_ptr argument, int min_computation_rank);

    virtual std::vector<operation_state_ptr> arguments() const;

    virtual std::string get_call_code_nd(const symbol_table_t& symbol_table, const node_to_info_t& node_to_info) const;

    virtual std::string kernel_name() const = 0;
};

struct AllReducerOperationState : public ReducerOperationState {
    static const hash_t optype_hash_cache_;

    virtual hash_t optype_hash() const;

    AllReducerOperationState(const std::string& functor_name, operation_state_ptr argument);

    virtual std::vector<int> bshape() const;

    virtual DType dtype() const;

    virtual int ndim() const;

    virtual void compute_node_compilation_info(int desired_computation_rank,
                                               const std::vector<int>& desired_computation_shape,
                                               std::vector<const ArrayOperationState*>* arrays,
                                               std::vector<const ScalarOperationState*>* scalars,
                                               node_to_info_t* node_to_info) const;

    virtual bool is_dim_collapsible_with_dim_minus_one(const int& dim) const;

    virtual operation_state_ptr transpose(const std::vector<int>& permutation) const;

    virtual std::string prefix_code(const node_to_info_t& node_to_info) const;

    virtual std::string kernel_name() const;
};

struct AxisReducerOperationState : public ReducerOperationState {
    static const hash_t optype_hash_cache_;

    virtual hash_t optype_hash() const;

    AxisReducerOperationState(const std::string& functor_name, operation_state_ptr argument);

    virtual std::vector<int> bshape() const;

    virtual DType dtype() const;

    virtual int ndim() const;

    virtual void compute_node_compilation_info(
        int desired_computation_rank,
        const std::vector<int>& desired_computation_shape,
        std::vector<const ArrayOperationState*>* arrays,
        std::vector<const ScalarOperationState*>* scalars,
        node_to_info_t* node_to_info) const;

    virtual bool is_dim_collapsible_with_dim_minus_one(const int& dim) const;

    virtual operation_state_ptr collapse_dim_with_dim_minus_one(const int& dim) const;

    virtual operation_state_ptr transpose(const std::vector<int>& permutation) const;

    virtual std::string prefix_code(const node_to_info_t& node_to_info) const;

    virtual std::string kernel_name() const;
};


struct ArgumentAllReducerOperationState : public AllReducerOperationState {
    static const hash_t optype_hash_cache_;

    using AllReducerOperationState::AllReducerOperationState;

    virtual hash_t optype_hash() const;

    virtual DType dtype() const;

    virtual std::string prefix_code(const node_to_info_t& node_to_info) const;

    virtual std::string kernel_name() const;
};


struct ArgumentAxisReducerOperationState : public AxisReducerOperationState {
    static const hash_t optype_hash_cache_;

    using AxisReducerOperationState::AxisReducerOperationState;

    virtual hash_t optype_hash() const;

    virtual DType dtype() const;

    virtual std::string prefix_code(const node_to_info_t& node_to_info) const;

    virtual operation_state_ptr collapse_dim_with_dim_minus_one(const int& dim) const;

    virtual operation_state_ptr transpose(const std::vector<int>& permutation) const;

    virtual std::string kernel_name() const;
};


///////////////////////////////////////////////////////////////////////////////
//                    ALL REDUCER OPERATION STATE                            //
///////////////////////////////////////////////////////////////////////////////


const hash_t AxisReducerOperationState::optype_hash_cache_ = std::hash<std::string>()("AxisReducerOperationState");

hash_t AllReducerOperationState::optype_hash() const {
    return optype_hash_cache_;
}

AllReducerOperationState::AllReducerOperationState(
        const std::string& functor_name,
        operation_state_ptr argument) :
    ReducerOperationState(functor_name, argument, 1) {
}

std::vector<int> AllReducerOperationState::bshape() const {
    return {};
}

DType AllReducerOperationState::dtype() const {
    return argument_->dtype();
}

int AllReducerOperationState::ndim() const {
    return 0;
}

void AllReducerOperationState::compute_node_compilation_info(
        int desired_computation_rank,
        const std::vector<int>& desired_computation_shape,
        std::vector<const ArrayOperationState*>* arrays,
        std::vector<const ScalarOperationState*>* scalars,
        node_to_info_t* node_to_info) const {
    (*node_to_info)[this].computation_rank = desired_computation_rank;
    argument_->compute_node_compilation_info(argument_->min_computation_rank_, argument_->shape(), arrays, scalars, node_to_info);
    (*node_to_info)[this].hash = utils::Hasher().add(optype_hash())
                                                .add(desired_computation_rank)
                                                .add(functor_name_)
                                                .add(node_to_info->at(argument_.get()).hash)
                                                .value();
}


bool AllReducerOperationState::is_dim_collapsible_with_dim_minus_one(
        const int& dim) const {
    return true;
}

operation_state_ptr AllReducerOperationState::transpose(
        const std::vector<int>& permutation) const {
    return shared_from_this();
}

std::string AllReducerOperationState::prefix_code(
        const node_to_info_t& node_to_info) const {
    return create_all_reduce_kernel_caller(
        node_to_info.at(argument_.get()).computation_rank,
        node_to_info.at(this).computation_rank
    );
}

std::string AllReducerOperationState::kernel_name() const {
    return "all_reduce_kernel_";
}



///////////////////////////////////////////////////////////////////////////////
//                    AXIS REDUCER OPERATION STATE                           //
///////////////////////////////////////////////////////////////////////////////

const hash_t ArgumentAllReducerOperationState::optype_hash_cache_ = std::hash<std::string>()("ArgumentAllReducerOperationState");

hash_t AxisReducerOperationState::optype_hash() const {
    return optype_hash_cache_;
}


AxisReducerOperationState::AxisReducerOperationState(
        const std::string& functor_name,
        operation_state_ptr argument) :
    ReducerOperationState(functor_name, argument, std::max(argument->min_computation_rank_ - 1, 1)) {
}

std::vector<int> AxisReducerOperationState::bshape() const {
    auto result = argument_->bshape();
    result.pop_back();
    return result;
}

DType AxisReducerOperationState::dtype() const {
    return argument_->dtype();
}

int AxisReducerOperationState::ndim() const {
    return std::max(argument_->ndim() - 1, 0);
}

void AxisReducerOperationState::compute_node_compilation_info(
        int desired_computation_rank,
        const std::vector<int>& desired_computation_shape,
        std::vector<const ArrayOperationState*>* arrays,
        std::vector<const ScalarOperationState*>* scalars,
        node_to_info_t* node_to_info) const {
    (*node_to_info)[this].computation_rank = desired_computation_rank;

    auto desired_argument_shape = desired_computation_shape;
    desired_argument_shape.emplace_back(argument_->shape().back());


    argument_->compute_node_compilation_info(desired_computation_rank + 1, desired_argument_shape, arrays, scalars, node_to_info);

    (*node_to_info)[this].hash = utils::Hasher().add(optype_hash())
                                                .add(desired_computation_rank)
                                                .add(functor_name_)
                                                .add(node_to_info->at(argument_.get()).hash)
                                                .value();
}

bool AxisReducerOperationState::is_dim_collapsible_with_dim_minus_one(
        const int& dim) const {
    return argument_->is_dim_collapsible_with_dim_minus_one(dim - 1);;
}

operation_state_ptr AxisReducerOperationState::collapse_dim_with_dim_minus_one(
        const int& dim) const {
    return std::make_shared<AxisReducerOperationState>(
        functor_name_,
        argument_->collapse_dim_with_dim_minus_one(dim - 1)
    );
}

operation_state_ptr AxisReducerOperationState::transpose(
        const std::vector<int>& permutation) const {
    auto new_permutation = permutation;
    // add last dim of tensor with rank (permutation.size() + 1)
    new_permutation.emplace_back(permutation.size());

    return std::make_shared<AxisReducerOperationState>(
        functor_name_,
        argument_->transpose(new_permutation)
    );
}

std::string AxisReducerOperationState::prefix_code(
        const node_to_info_t& node_to_info) const {
    return create_axis_reduce_kernel_caller(node_to_info.at(argument_.get()).computation_rank);
}

std::string AxisReducerOperationState::kernel_name() const {
    return "axis_reduce_kernel_";
}


///////////////////////////////////////////////////////////////////////////////
//                ARGUMENT ALL REDUCER OPERATION STATE                       //
///////////////////////////////////////////////////////////////////////////////



hash_t ArgumentAllReducerOperationState::optype_hash() const {
    return optype_hash_cache_;
}

DType ArgumentAllReducerOperationState::dtype() const {
    return DTYPE_INT32;
}

std::string ArgumentAllReducerOperationState::prefix_code(
        const node_to_info_t& node_to_info) const {
    return create_argument_all_reduce_kernel_caller(
        node_to_info.at(argument_.get()).computation_rank,
        node_to_info.at(this).computation_rank
    );
}

std::string ArgumentAllReducerOperationState::kernel_name() const {
    return "argument_all_reduce_kernel_";
}


///////////////////////////////////////////////////////////////////////////////
//         ARGUMENT AXIS REDUCER OPERATION STATE                             //
///////////////////////////////////////////////////////////////////////////////

const hash_t ArgumentAxisReducerOperationState::optype_hash_cache_ = std::hash<std::string>()("ArgumentAxisReducerOperationState");

hash_t ArgumentAxisReducerOperationState::optype_hash() const {
    return optype_hash_cache_;
}

DType ArgumentAxisReducerOperationState::dtype() const {
    return DTYPE_INT32;
}

std::string ArgumentAxisReducerOperationState::prefix_code(
        const node_to_info_t& node_to_info) const {
    return create_argument_axis_reduce_kernel_caller(node_to_info.at(argument_.get()).computation_rank);
}

operation_state_ptr ArgumentAxisReducerOperationState::collapse_dim_with_dim_minus_one(
        const int& dim) const {
    return std::make_shared<ArgumentAxisReducerOperationState>(
        functor_name_,
        argument_->collapse_dim_with_dim_minus_one(dim - 1)
    );
}

operation_state_ptr ArgumentAxisReducerOperationState::transpose(
        const std::vector<int>& permutation) const {
    auto new_permutation = permutation;
    // add last dim of tensor with rank (permutation.size() + 1)
    new_permutation.emplace_back(permutation.size());

    return std::make_shared<ArgumentAxisReducerOperationState>(
        functor_name_,
        argument_->transpose(new_permutation)
    );
}

std::string ArgumentAxisReducerOperationState::kernel_name() const {
    return "argument_axis_reduce_kernel_";
}


///////////////////////////////////////////////////////////////////////////////
//                       REDUCER OPERATION STATE                             //
///////////////////////////////////////////////////////////////////////////////

const hash_t AllReducerOperationState::optype_hash_cache_ =
        std::hash<std::string>()("AllReducerOperationState");

ReducerOperationState::ReducerOperationState(
        const std::string& functor_name,
        operation_state_ptr argument,
        int min_computation_rank) :
    OperationState(min_computation_rank),
    functor_name_(functor_name),
    argument_(argument) {

}

std::vector<operation_state_ptr> ReducerOperationState::arguments() const {
    return {argument_};
}

std::string ReducerOperationState::get_call_code_nd(
        const symbol_table_t& symbol_table,
        const node_to_info_t& node_to_info) const {
    int all_reduce_comp_rank = node_to_info.at(argument_.get()).computation_rank;
    return utils::make_message(
        kernel_name(), all_reduce_comp_rank,
        "d<", functor_name_, ", " , dtype_to_cpp_name(dtype()) , ">(",
        argument_->get_call_code_nd(symbol_table, node_to_info), ")");

}


namespace op2 {
    Operation all_reduce(const Operation& a,
                         const std::string& reducer_name) {
        return Operation(std::make_shared<AllReducerOperationState>(
            reducer_name,
            a.state_
        ));
    }

    Operation axis_reduce(const Operation& a,
                          const std::string& reducer_name,
                          const std::vector<int>& axes) {
        if (axes.size() == 0) return a;
        int ndim = a.ndim();
        if (ndim == 0) return a;
        std::vector<int> normalized_axes(axes);
        for (auto& axis : normalized_axes) {
            if (axis < 0) {
                if (ndim == 0) {
                    axis = axis + 1;
                } else {
                    axis = axis + ndim;
                }
            }
            ASSERT2(axis >= 0 && (axis < ndim || ndim == 0 && axis == ndim),
                utils::make_message(
                    "Reduction axis must strictly positive and less than the "
                    "number of dimensions of the input (got axis=", axes[0], ","
                    " ndim=", ndim, ")."
                )
            );
        }
        // now look to see what kind of a reduction this is:
        std::vector<bool> reduced_dims(ndim, false);
        std::sort(normalized_axes.begin(), normalized_axes.end());
        for (auto& axis : normalized_axes) {
            ASSERT2(!reduced_dims[axis], utils::make_message("axis_reduce "
                "received duplicate axes to operate on (axis=", axis,
                " axes=", axes, ")."
            ));
            reduced_dims[axis] = true;
        }
        // all axes are present:
        if (normalized_axes.size() == ndim) {
            return all_reduce(a, reducer_name);
        }
        int num_low_dims = 0;
        for (int i = reduced_dims.size() - 1; i >= 0; --i) {
            if (reduced_dims[i]) {
                ++num_low_dims;
            } else {
                break;
            }
        }
        bool all_reductions_are_low_dim = num_low_dims == normalized_axes.size();
        auto res = a;

        if (!all_reductions_are_low_dim) {
            std::vector<int> new_axes_order;
            for (int i = 0; i < reduced_dims.size(); ++i) {
                if (!reduced_dims[i]) {
                    new_axes_order.emplace_back(i);
                }
            }
            for (int i = 0; i < reduced_dims.size(); ++i) {
                if (reduced_dims[i]) {
                    new_axes_order.emplace_back(i);
                }
            }
            res = res.transpose(new_axes_order);
        }
        int num_low_axes_to_reduce = normalized_axes.size();
        if (num_low_axes_to_reduce > 0) {
            int axes_used_up = 0;
            int collapsed_ndim = ndim - 1;
            for (int axes_used_up = 0; axes_used_up < num_low_axes_to_reduce; ++axes_used_up) {
                if (num_low_axes_to_reduce - axes_used_up == 1) {
                    res = Operation(std::make_shared<AxisReducerOperationState>(
                        reducer_name,
                        res.state_
                    ));
                } else {
                    if (res.is_dim_collapsible_with_dim_minus_one(collapsed_ndim)) {
                        res = res.collapse_dim_with_dim_minus_one(collapsed_ndim);
                    } else {
                        res = Operation(std::make_shared<AxisReducerOperationState>(
                            reducer_name,
                            res.state_
                        ));
                    }
                }
                --collapsed_ndim;
            }
        }
        return res;
    }

    Operation argument_all_reduce(const Operation& a,
                                 const std::string& reducer_name) {
        return Operation(std::make_shared<ArgumentAllReducerOperationState>(
            reducer_name,
            a.state_
        ));
    }

    Operation argument_axis_reduce(const Operation& a,
                                   const std::string& reducer_name,
                                   const int& axis) {
        int ndim = a.ndim();
        if (ndim == 0) return Operation(0);
        int normalized_axis = axis;
        if (normalized_axis < 0) normalized_axis = normalized_axis + a.ndim();
        ASSERT2(normalized_axis >= 0 && (normalized_axis < ndim || ndim == 0 && normalized_axis == ndim),
            utils::make_message(
                "Reduction axis must strictly positive and less than the "
                "number of dimensions of the input (got axis=", normalized_axis, ","
                " ndim=", ndim, ")."
            )
        );
        if (ndim == 1) return argument_all_reduce(a, reducer_name);

        auto res = a;
        if (normalized_axis != ndim - 1) {
            std::vector<int> axes;
            for (int i = 0; i < ndim; i++) {
                axes.emplace_back(i);
            }
            axes[axes.size() - 1] = normalized_axis;
            axes[normalized_axis] = axes.size() - 1;
            res = res.transpose(axes);
        }
        return Operation(std::make_shared<ArgumentAxisReducerOperationState>(
            reducer_name,
            res.state_
        ));
    }

    Operation sum(const Operation& x) {
        return all_reduce(x, "reducers::sum");
    }
    Operation sum(const Operation& x, const std::vector<int>& axes) {
        return axis_reduce(x, "reducers::sum", axes);
    }
    Operation prod(const Operation& x) {
        return all_reduce(x, "reducers::product");
    }
    Operation prod(const Operation& x, const std::vector<int>& axes) {
        return axis_reduce(x, "reducers::product", axes);
    }
    Operation max(const Operation& x) {
        return all_reduce(x, "reducers::maximum");
    }
    Operation max(const Operation& x, const std::vector<int>& axes) {
        return axis_reduce(x, "reducers::maximum", axes);
    }
    Operation min(const Operation& x) {
        return all_reduce(x, "reducers::minimum");
    }
    Operation min(const Operation& x, const std::vector<int>& axes) {
        return axis_reduce(x, "reducers::minimum", axes);
    }
    Operation mean(const Operation& x) {
        auto sum_op = all_reduce(x, "reducers::sum");
        if (sum_op.dtype() == DTYPE_INT32) sum_op = astype(sum_op, DTYPE_DOUBLE);
        return op2::eltdiv(sum_op, x.number_of_elements());
    }
    Operation mean(const Operation& x, const std::vector<int>& axes) {
        auto sum_op = axis_reduce(x, "reducers::sum", axes);
        if (sum_op.dtype() == DTYPE_INT32) sum_op = astype(sum_op, DTYPE_DOUBLE);
        return op2::eltdiv(sum_op, x.number_of_elements() / sum_op.number_of_elements());
    }
    Operation L2_norm(const Operation& x) {
        auto sum_op = all_reduce(op2::square(x), "reducers::sum");
        if (sum_op.dtype() == DTYPE_INT32) sum_op = astype(sum_op, DTYPE_DOUBLE);
        return op2::sqrt(sum_op);
    }
    Operation L2_norm(const Operation& x, const std::vector<int>& axes) {
        auto sum_op = axis_reduce(op2::square(x), "reducers::sum", axes);
        if (sum_op.dtype() == DTYPE_INT32) sum_op = astype(sum_op, DTYPE_DOUBLE);
        return op2::sqrt(sum_op);
    }
    Operation argmax(const Operation& x) {
        return argument_all_reduce(x, "reducers::maximum");
    }
    Operation argmax(const Operation& x, const int& axis) {
        return argument_axis_reduce(x, "reducers::maximum", axis);
    }
    Operation argmin(const Operation& x) {
        return argument_all_reduce(x, "reducers::minimum");
    }
    Operation argmin(const Operation& x, const int& axis) {
        return argument_axis_reduce(x, "reducers::minimum", axis);
    }
}
