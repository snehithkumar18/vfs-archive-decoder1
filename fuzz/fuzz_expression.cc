#include "../src/expression_eval.h"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > 8192) {
        return 0;
    }

    std::string expression(reinterpret_cast<const char*>(data), size);
    PixelForge::ExpressionContext context;
    PixelForge::ExpressionEvaluator evaluator(context);
    evaluator.evaluate(expression);
    return 0;
}
