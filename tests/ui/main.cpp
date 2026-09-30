#include "check.h"

#include <imgui.h>

#include <cstring>

TEST(imgui_version_matches_pin)
{
    CHECK(std::strcmp(IMGUI_VERSION, "1.92.6") == 0);
}

TEST(imgui_context_creates_headless)
{
    ImGuiContext* context = ImGui::CreateContext();
    CHECK(context != nullptr);
    ImGui::DestroyContext(context);
}

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
