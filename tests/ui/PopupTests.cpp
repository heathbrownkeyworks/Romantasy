#include "check.h"

#include "ui/screens/LevelUpPopup.h"

using namespace romantasy::ui;

TEST(popup_reset_starts_scale_in_and_clears_dismiss)
{
    PopupViewState view;
    view.dismissing = true;
    view.lifetime = 5.0f;
    view.Reset();
    CHECK(view.in.Running());
    CHECK(!view.dismissing);
    CHECK_NEAR(view.lifetime, 0.0f, 1e-6);
    CHECK(!view.out.Running());
}

TEST(popup_dismiss_starts_fade_out_once)
{
    PopupViewState view;
    view.Reset();
    view.Dismiss();
    CHECK(view.dismissing);
    CHECK(view.out.Running());
    view.out.Advance(0.1f);
    view.Dismiss();  // second call must not restart the fade
    CHECK_NEAR(view.out.elapsed, 0.1f, 1e-6);
}

TEST(popup_auto_dismiss_constant_is_twenty_seconds)
{
    CHECK_NEAR(PopupAutoDismissSeconds, 20.0f, 1e-6);
}
