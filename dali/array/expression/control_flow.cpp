#include "control_flow.h"
// TODO should pass strides + offset to Expression
ControlFlow::ControlFlow(Array left, const std::vector<Array>& conditions) :
        Expression(left.shape(),
                   left.dtype()),
                   left_(left), conditions_(conditions) {

}

ControlFlow::ControlFlow(const ControlFlow& other) :
        ControlFlow(other.left_, other.conditions_) {
}

expression_ptr ControlFlow::copy() const {
	return std::make_shared<ControlFlow>(*this);
}

memory::Device ControlFlow::preferred_device() const {
	return left_.preferred_device();
}

std::vector<Array> ControlFlow::arguments() const {
	std::vector<Array> args({left_,});
	args.insert(args.begin(), conditions_.begin(), conditions_.end());
	return args;
}