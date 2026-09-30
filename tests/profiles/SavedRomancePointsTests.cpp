#include "check.h"
#include "romance/SavedRomancePoints.h"

using namespace romantasy;

TEST(saved_zero_overrides_initial_and_legacy_points)
{
    const std::unordered_map<std::uint32_t, std::int32_t> saved = {{0x1234, 0}, {0x800, 2000}};
    CHECK(RestorePoints(saved, 0x1234, 0x800, true, 1000) == 0);
    CHECK(RestorePoints(saved, 0x1234, 0x800, false, 1000) == 0);
}

TEST(instances_share_defaults_but_never_saved_balances)
{
    const std::unordered_map<std::uint32_t, std::int32_t> saved = {
        {0xFF001234, 750}, {0xFF001235, 2125}, {0x800, 2500}};
    CHECK(RestorePoints(saved, 0xFF001234, 0x800, false, 500) == 750);
    CHECK(RestorePoints(saved, 0xFF001235, 0x800, false, 500) == 2125);
    CHECK(RestorePoints(saved, 0xFF001236, 0x800, false, 500) == 500);
}

TEST(legacy_base_migration_and_new_save_defaults)
{
    const std::unordered_map<std::uint32_t, std::int32_t> oldSave = {{0x800, 1750}};
    CHECK(RestorePoints(oldSave, 0x1234, 0x800, true, 500) == 1750);
    CHECK(RestorePoints(oldSave, 0, 0x800, true, 500) == 1750);
    CHECK(RestorePoints({}, 0x1234, 0x800, true, 500) == 500);
}
