#pragma once

#include "romance/Journal.h"

#include <string_view>

#include <nlohmann/json.hpp>

namespace romantasy::ui
{
    // The romantasyJournal message for one bond. With a journal: found true plus its lists, every day as a
    // "Day N" number. With nullptr: found false. Strings are sent as they are; dump with the replace error handler.
    // firstMomentDay (a game day, below 0 for never): the first shared moment, placed among the milestones as
    // { kind "shared", level 0 } before the first one of a later day. It is derived here, never stored in the journal.
    [[nodiscard]] nlohmann::json JournalJson(
        std::string_view identity, const journal::Journal* journal, float daysTogether, float firstMomentDay = -1.0f);
}
