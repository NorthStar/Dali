#include "reshape.h"
#include "dali/array/jit/jit_runner.h"
#include "dali/array/jit/jit_utils.h"
#include "dali/utils/make_message.h"

namespace op {
namespace jit {

struct ReshapeRestride : public JITNode {
    static const hash_t optype_hash;
    ReshapeRestride(Array array, const std::vector<int>& shape,
                    int offset, const std::vector<int>& strides) :
        JITNode(min_computation_rank(array), shape, array.dtype(), {array}, offset, strides) {
    }

    virtual memory::Device preferred_device() const {
        return arguments_[0].preferred_device();
    }

    virtual std::string get_call_code_nd(const SymbolTable& symbol_table,
                                         const node_to_info_t& node_to_info,
                                         memory::DeviceT device_type) const {
        return generate_call_code_nd(this,
                                     kernel_name(node_to_info),
                                     symbol_table, node_to_info, device_type,
                                     /*has_shape=*/true);
    }

    virtual expression_ptr _reshape(const std::vector<int>& new_shape, const Array* owner) const {
        return std::make_shared<ReshapeRestride>(arguments_[0], new_shape, offset_, strides_);
    }

    virtual void compute_node_compilation_info(int desired_computation_rank,
                                               const std::vector<int>& desired_computation_shape,
                                               SymbolTable& symbol_table,
                                               node_to_info_t* node_to_info) const {
        (*node_to_info)[this].computation_rank = desired_computation_rank;
        (*node_to_info)[this].computation_shape = desired_computation_shape;
        symbol_table.declare_shape(this);
        op::jit::compute_node_compilation_info(arguments_[0],
                                               std::max(1, arguments_[0].ndim()),
                                               arguments_[0].ndim() == 0 ? std::vector<int>({1}) : arguments_[0].shape(),
                                               symbol_table,
                                               node_to_info);
        utils::Hasher hasher;
        hasher.add(optype_hash)
              .add(desired_computation_rank)
              .add(node_to_info->at(arguments_[0].expression().get()).hash);
        (*node_to_info)[this].hash = hasher.value();
    }

    virtual std::string kernel_name(const node_to_info_t& node_to_info) const {
        return utils::make_message("reshape", node_to_info.at(this).computation_rank, "d");
    }

    virtual std::string prefix_code(const node_to_info_t& node_to_info,
                                    memory::DeviceT device_type) const {
        return define_kernel(/*ndim=*/node_to_info.at(this).computation_rank,
                             /*has_shape=*/true,
                             /*arguments=*/{"array",},
                             /*kernel=*/"array_[index_to_dim(indices_to_offset(shape_, query), array_.shape())]",
                             /*name=*/kernel_name(node_to_info),
                             /*is_assignable=*/false);
    }

    virtual expression_ptr copy() const {
        return std::make_shared<ReshapeRestride>(arguments_[0], shape_, offset_, strides_);
    }
};

const hash_t ReshapeRestride::optype_hash = std::hash<std::string>()(typeid(ReshapeRestride).name());

Array jit_view(const Array& array,
               const std::vector<int>& shape,
               int offset,
               const std::vector<int>& strides) {
    return Array(std::make_shared<ReshapeRestride>(array, shape, offset, strides));
}



struct BroadcastedReshape : public JITNode {
    static const hash_t optype_hash;
    std::vector<bool> broadcasted_;
    BroadcastedReshape(Array array, const std::vector<int>& shape,
                       const std::vector<bool>& broadcasted) :
        JITNode(shape.size(), shape, array.dtype(), {array}),
        broadcasted_(broadcasted) {}

    virtual memory::Device preferred_device() const {
        return arguments_[0].preferred_device();
    }

    virtual std::string get_call_code_nd(const SymbolTable& symbol_table,
                                         const node_to_info_t& node_to_info,
                                         memory::DeviceT device_type) const {
        return generate_call_code_nd(this,
                                     kernel_name(node_to_info),
                                     symbol_table, node_to_info, device_type,
                                     /*has_shape=*/true);
    }

    virtual void compute_node_compilation_info(int desired_computation_rank,
                                               const std::vector<int>& desired_computation_shape,
                                               SymbolTable& symbol_table,
                                               node_to_info_t* node_to_info) const {
        (*node_to_info)[this].computation_rank = desired_computation_rank;
        (*node_to_info)[this].computation_shape = desired_computation_shape;
        symbol_table.declare_shape(this);
        op::jit::compute_node_compilation_info(arguments_[0],
                                               desired_computation_rank,
                                               arguments_[0].shape(),
                                               symbol_table,
                                               node_to_info);
        utils::Hasher hasher;
        hasher.add(optype_hash)
              .add(desired_computation_rank);
        for (auto val : broadcasted_) {
            hasher.add(int(val));
        }
        hasher.add(node_to_info->at(arguments_[0].expression().get()).hash);
        (*node_to_info)[this].hash = hasher.value();
    }

    std::string bool_encoding(const node_to_info_t& node_to_info) const {
        std::stringstream ss;
        int ndim = node_to_info.at(this).computation_rank;
        int prefix = ndim - broadcasted_.size();
        for (int i = 0; i < ndim; i++) {
            ss << ((i >= prefix && broadcasted_[i - prefix]) ? "T" : "F");
        }
        return ss.str();
    }

    virtual std::string kernel_name(const node_to_info_t& node_to_info) const {
        return utils::make_message("broadcasted_reshape", bool_encoding(node_to_info));
    }

    virtual std::string prefix_code(const node_to_info_t& node_to_info,
                                    memory::DeviceT device_type) const {
        int ndim = node_to_info.at(this).computation_rank;
        std::stringstream ss;
        int prefix = ndim - broadcasted_.size();
        for (int i = 0; i < ndim; i++) {
            if (i >= prefix && broadcasted_[i - prefix]) {
                ss << "0";
            } else {
                ss << "query[" << i << "]";
            }
            if (i + 1 < ndim) {
                ss << ", ";
            }
        }
        auto broadcasted_access = ss.str();
        return define_kernel(/*ndim=*/ndim,
                             /*has_shape=*/true,
                             /*arguments=*/{"array",},
                             /*kernel=*/utils::make_message("array_[{", broadcasted_access, "}]"),
                             /*name=*/kernel_name(node_to_info),
                             /*is_assignable=*/false);
    }

    virtual expression_ptr copy() const {
        return std::make_shared<BroadcastedReshape>(arguments_[0], shape_, broadcasted_);
    }
};

const hash_t BroadcastedReshape::optype_hash = std::hash<std::string>()(typeid(BroadcastedReshape).name());

Array broadcasted_reshape(const Array& array,
                          const std::vector<int>& shape) {
    if (array.shape() == shape) {
        return array;
    }
    const auto& current_shape = array.shape();
    ASSERT2(current_shape.size() == shape.size(), utils::make_message(
        "new_shape for broadcasted_reshape must have the same dimensionality "
        "as the current shape (current_shape = ", current_shape,
        ", new_shape = ", shape, ")."));
    std::vector<bool> broadcasted(current_shape.size(), 0);
    for (int i = 0; i < current_shape.size(); i++) {
        if (current_shape[i] != shape[i]) {
            ASSERT2(current_shape[i] == 1, utils::make_message(
                "broadcasted dimension must have size 1, but on axis ", i,
                " got dimension with size ", current_shape[i], " (current_shape = ",
                current_shape, ", new_shape = ", shape, ")."));
            broadcasted[i] = true;
        } else {
            broadcasted[i] = false;
        }
    }
    return Array(std::make_shared<BroadcastedReshape>(array, shape, broadcasted));
}
}  // namespace jit
}  // namespace op
