#include "gather_from_rows.h"

#include "dali/array/op2/operation.h"
#include "dali/array/op2/elementwise_operation.h"
#include "dali/array/op2/rtc_utils.h"
#include "dali/utils/hash_utils.h"
#include "dali/utils/make_message.h"

struct GatherFromRowsState : public OperationState {
    static const hash_t optype_hash;

    operation_state_ptr source_;
    operation_state_ptr indices_;

    GatherFromRowsState(operation_state_ptr source, operation_state_ptr indices) :
            OperationState(
                // operation requires source to not be collapsed to perform
                // correct gathers
                source->ndim() - 1
            ),
            source_(source),
            indices_(indices) {
    }

    virtual std::string caller_function_name(const node_to_info_t& node_to_info) const {
        int source_computation_rank = node_to_info.at(this).computation_rank;
        return utils::make_message("gather_from_rows_kernel_", source_computation_rank, "d");
    }

    std::string prefix_code(const node_to_info_t& node_to_info) const {
        int source_computation_rank = node_to_info.at(source_.get()).computation_rank;
        int indices_computation_rank = node_to_info.at(indices_.get()).computation_rank;
        int self_computation_rank = node_to_info.at(this).computation_rank;
        std::string name = utils::make_message("GatherFromRowsKernel", self_computation_rank ,"D");
        std::stringstream ss;
        ss << "{query[0]";
        ASSERT2(indices_computation_rank == 1,
            utils::make_message("computation_rank for indices should be 1 (got rank=",
            indices_computation_rank, ").")
        );
        ss << ", indices_(query[0])";
        for (int i = 1; i < source_computation_rank - 1; i++) {
            ss << ", query[" << i << "]";
        }
        ss << "}";
        std::string nd_access = ss.str();
        std::string one_dimensional_access = "";
        if (self_computation_rank == 1) {
            one_dimensional_access = (
        "    XINLINE T operator()(int index) {\n"
        "        return source_[{index, indices_(index)}];\n"
        "    }\n"
            );
        }
        return utils::make_message("template<typename C1, typename C2>\n"
        "struct ", name, " {\n"
        "    C1 source_;\n"
        "    C2 indices_;\n"
        "    static const int ndim = C1::ndim - 1;\n"
        "    typedef typename C1::T T;\n"
        "    XINLINE ", name, "(const C1& source, const C2& indices)\n"
        "        : source_(source), indices_(indices) {}\n"
        "    XINLINE T operator[](const Shape<ndim>& query) {\n"
        "        return source_[", nd_access, "];\n"
        "    }\n", one_dimensional_access,
        "};\n"
        "template<typename C1, typename C2>\n",
        name, "<C1, C2> ", caller_function_name(node_to_info), "(const C1& a, const C2& b) {\n"
        "    return ", name, "<C1, C2>(a, b);\n"
        "}\n");
    }

    DType dtype() const {
        return source_->dtype();
    }

    std::vector<int> bshape() const {
        auto indices_bshape = indices_->bshape();
        auto source_bshape = source_->bshape();
        std::vector<int> res(source_bshape.begin() + 2, source_bshape.end());
        if (indices_bshape.size() > 0) {
            res.insert(res.begin(), indices_bshape.begin(), indices_bshape.end());
        } else {
            res.insert(res.begin(), 1);
        }
        return res;
    }

    virtual int ndim() const {
        return source_->ndim() - 1;
    }

    std::vector<operation_state_ptr> arguments() const {
        return {source_, indices_};
    }

    bool is_dim_collapsible_with_dim_minus_one(const int& dim) const {
        return false;
    }

    operation_state_ptr collapse_dim_with_dim_minus_one(const int& dim) const {
        // TODO(jonathan): there is a way to transpose the index dimensions of
        // gather, or the non-leading dimension of the source.
        throw std::runtime_error(
            "Cannot transpose gather (yet)."
        );
        return shared_from_this();
    }

    operation_state_ptr transpose(const std::vector<int>& permutation) const {
        // TODO(jonathan): there is a way to transpose the index dimensions of
        // gather, or the non-leading dimension of the source.
        throw std::runtime_error(
            "Cannot transpose gather (yet)."
        );
        return shared_from_this();
    }

    void compute_node_compilation_info(
            int desired_computation_rank,
            const std::vector<int>& desired_computation_shape,
            std::vector<const ArrayOperationState*>* arrays,
            std::vector<const ScalarOperationState*>* scalars,
            node_to_info_t* node_to_info) const {
        (*node_to_info)[this].computation_rank = desired_computation_rank;

        auto source_original_bshape = source_->bshape();
        int source_ndim = source_original_bshape.size();
        // indices dim 1, dim2, etc... source dim 3, dim 4, etc...
        std::vector<int> source_shape(
            desired_computation_shape.end() - (source_ndim - 2),
            desired_computation_shape.end()
        );
        // add dim 1 & 2 of source back in (hidden from output by gather operation).
        if (source_original_bshape[0] == -1) {
            source_original_bshape[0] = desired_computation_shape[0];
        }
        if (source_original_bshape[1] == -1) {
            source_original_bshape[1] = desired_computation_shape[0];
        }
        source_shape.insert(source_shape.begin(), source_original_bshape.begin(), source_original_bshape.begin() + 2);
        std::vector<int> indices_shape(
            desired_computation_shape.begin(),
            desired_computation_shape.end() - (source_ndim - 2)
        );

        source_->compute_node_compilation_info(source_ndim, source_shape, arrays, scalars, node_to_info);
        indices_->compute_node_compilation_info(1, indices_shape, arrays, scalars, node_to_info);
        (*node_to_info)[this].hash = utils::Hasher().add(optype_hash)
                                                    .add(desired_computation_rank)
                                                    .add(node_to_info->at(source_.get()).hash)
                                                    .add(node_to_info->at(indices_.get()).hash)
                                                    .value();
    }

    std::string get_call_code_nd(
            const symbol_table_t& symbol_table,
            const node_to_info_t& node_to_info) const {
        return utils::make_message(caller_function_name(node_to_info), "(",
                                    source_->get_call_code_nd(symbol_table, node_to_info),
                                    ",",
                                    indices_->get_call_code_nd(symbol_table, node_to_info),
                                    ")");
    }
};

const hash_t GatherFromRowsState::optype_hash = std::hash<std::string>()("GatherFromRowsState");

namespace op2 {
    Operation gather_from_rows(const Operation& source, const Operation& indices) {
        ASSERT2(
            source.ndim() > 1,
            utils::make_message("gather must be called on source with ndim >="
                " 2 (got ndim=", source.ndim(), ").")
        );
        ASSERT2(
            indices.dtype() == DTYPE_INT32,
            utils::make_message("indices must be integers (got dtype=", indices.dtype(), ").")
        );
        ASSERT2(
            indices.ndim() <= 1,
            utils::make_message("indices must have rank 1 or lower [Note: support for "
                "higher ranks coming soon] (got indices.ndim=", indices.ndim(), ").")
        );
        auto index_bshape = indices.bshape();
        auto source_bshape = source.bshape();
        if (index_bshape.size() > 0) {
            ASSERT2(index_bshape[0] <= source_bshape[0] || index_bshape[0] == -1 || source_bshape[0] == -1,
                utils::make_message("dimension 1 of indices must be less than or equal "
                    "to first dimension of source (got indices.shape[0]=", index_bshape[0],
                    ", source.shape[0]=", source_bshape[0], ")")
            );
        }
        return Operation(std::make_shared<GatherFromRowsState>(source.state_, indices.state_));
    }
}  // namespace op2
