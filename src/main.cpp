#include "first_app.hpp"
#include "log.hpp"
#include "ref.hpp"

using namespace vkao;

int main() {
    VKAO_ASSERT(true, "assert");
    auto app = vkao::make_ref<FirstApp>();
    app->run();
}