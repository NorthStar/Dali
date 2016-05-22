#ifndef DALI_ARRAY_DEBUG_H
#define DALI_ARRAY_DEBUG_H

#include "dali/utils/debug_callback.h"

class Array;

namespace debug {
    extern DebugCallback<bool>  dali_function_computed;
    extern DebugCallback<Array> lazy_evaluation_callback;
}  // namespace debug

#endif // DALI_ARRAY_DEBUG_H