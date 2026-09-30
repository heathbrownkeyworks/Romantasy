#include "check.h"

int main()
{
    int passed = 0;
    for (const auto& test : Tests()) {
        const int before = g_failures;
        test.fn();
        if (g_failures == before) {
            ++passed;
        } else {
            std::printf("FAILED: %s\n", test.name);
        }
    }
    std::printf("%d passed, %d failed\n", passed, g_failures);
    return g_failures == 0 ? 0 : 1;
}
