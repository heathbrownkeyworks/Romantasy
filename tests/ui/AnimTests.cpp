#include "check.h"

#include "ui/screens/Anim.h"

using namespace romantasy::ui::anim;

TEST(bezier_endpoints_are_exact)
{
    CHECK_NEAR(EaseOut(0.0f), 0.0f, 1e-5);
    CHECK_NEAR(EaseOut(1.0f), 1.0f, 1e-5);
    CHECK_NEAR(EaseSoft(0.0f), 0.0f, 1e-5);
    CHECK_NEAR(EaseSoft(1.0f), 1.0f, 1e-5);
}

TEST(bezier_clamps_out_of_range_input)
{
    CHECK_NEAR(EaseOut(-2.0f), 0.0f, 1e-5);
    CHECK_NEAR(EaseOut(3.0f), 1.0f, 1e-5);
}

TEST(bezier_ease_out_is_front_loaded_and_monotonic)
{
    CHECK(EaseOut(0.5f) > 0.85f);
    float previous = 0.0f;
    for (int i = 1; i <= 20; ++i) {
        const float value = EaseOut(static_cast<float>(i) / 20.0f);
        CHECK(value >= previous - 1e-5f);
        previous = value;
    }
}

TEST(bezier_linear_control_points_are_identity)
{
    const CubicBezier linear{ 0.0f, 0.0f, 1.0f, 1.0f };
    CHECK_NEAR(linear(0.25f), 0.25f, 1e-3);
    CHECK_NEAR(linear(0.75f), 0.75f, 1e-3);
}

TEST(tween_advances_and_finishes)
{
    Tween tween;
    CHECK(tween.Done());
    tween.Start(1.0f);
    CHECK(tween.Running());
    CHECK_NEAR(tween.Advance(0.25f), 0.25f, 1e-5);
    CHECK_NEAR(tween.Advance(0.5f), 0.75f, 1e-5);
    CHECK_NEAR(tween.Advance(1.0f), 1.0f, 1e-5);
    CHECK(!tween.Running());
    CHECK(tween.Done());
}

TEST(tween_finish_jumps_to_end)
{
    Tween tween;
    tween.Start(2.0f);
    tween.Finish();
    CHECK_NEAR(tween.Progress(), 1.0f, 1e-5);
    CHECK(tween.Done());
}

TEST(tween_zero_duration_is_complete)
{
    Tween tween;
    tween.Start(0.0f);
    CHECK_NEAR(tween.Progress(), 1.0f, 1e-5);
    CHECK(tween.Done());
}

TEST(pulse_loops_between_zero_and_one)
{
    CHECK_NEAR(Pulse(0.0f, 1.2f), 0.0f, 1e-5);
    CHECK_NEAR(Pulse(0.6f, 1.2f), 1.0f, 1e-5);
    CHECK_NEAR(Pulse(1.2f, 1.2f), 0.0f, 1e-4);
}
