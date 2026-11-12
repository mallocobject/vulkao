#include "first_app.hpp"
#include "logger.hpp"
#include "ref.hpp"

using namespace vkao;

int main() {
    VKAO_ASSERT(true, "assert");
    FirstApp app;
    app.run();
}