#include "AIProvider.hpp"

#include "ClaudeProvider.hpp"
#include "DeepSeekProvider.hpp"
#include "GLMProvider.hpp"
#include "OpenAIProvider.hpp"

namespace blog {
namespace ai {

AIProvider::ptr CreateProvider(const std::string& type)
{
    if (type == "claude") {
        return std::make_shared<ClaudeProvider>();
    }
    if (type == "deepseek") {
        return std::make_shared<DeepSeekProvider>();
    }
    if (type == "glm") {
        return std::make_shared<GLMProvider>();
    }
    return std::make_shared<OpenAIProvider>(); // 默认 OpenAI
}

} // namespace ai
} // namespace blog
