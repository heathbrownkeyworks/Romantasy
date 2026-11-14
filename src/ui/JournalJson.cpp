#include "ui/JournalJson.h"

#include <cmath>
#include <string>

namespace romantasy::ui
{
    namespace
    {
        using nlohmann::json;

        const char* KindName(journal::MilestoneKind kind)
        {
            switch (kind) {
                case journal::MilestoneKind::Began: return "began";
                case journal::MilestoneKind::Reached: return "reached";
                case journal::MilestoneKind::Fell: return "fell";
            }
            return nullptr;
        }

        json Changes(const std::vector<journal::Change>& list)
        {
            json out = json::array();
            for (const auto& change : list) {
                out.push_back({ { "label", change.label },
                    { "points", change.points },
                    { "day", journal::DayNumber(change.day) } });
            }
            return out;
        }
    }

    json JournalJson(std::string_view identity, const journal::Journal* journal, float daysTogether, float firstMomentDay)
    {
        json out = json::object();
        out["identity"] = std::string(identity);
        out["found"] = journal != nullptr;
        if (!journal) {
            return out;
        }

        std::int32_t began = 0;
        json milestones = json::array();
        for (const auto& m : journal->milestones) {
            const char* kind = KindName(m.kind);
            if (!kind) {
                continue;
            }
            const auto day = journal::DayNumber(m.day);
            if (m.kind == journal::MilestoneKind::Began && began == 0) {
                began = day;
            }
            milestones.push_back({ { "kind", kind }, { "level", m.level }, { "day", day } });
        }
        if (std::isfinite(firstMomentDay) && firstMomentDay >= 0.0f) {
            const auto day = journal::DayNumber(firstMomentDay);
            auto at = milestones.begin();
            while (at != milestones.end() && (*at)["day"].get<std::int32_t>() <= day) {
                ++at;
            }
            milestones.insert(at, json::object({ { "kind", "shared" }, { "level", 0 }, { "day", day } }));
        }

        json days = json::array();
        for (const auto& d : journal->days) {
            days.push_back({ { "day", journal::DayNumber(static_cast<float>(d.day)) },
                { "gained", d.gained },
                { "lost", d.lost } });
        }

        const double together = std::isfinite(daysTogether) && daysTogether > 0.0f
            ? std::round(static_cast<double>(daysTogether) * 10.0) / 10.0
            : 0.0;

        out["began"] = began;
        out["daysTogether"] = together;
        out["milestones"] = std::move(milestones);
        out["days"] = std::move(days);
        out["gains"] = Changes(journal->gains);
        out["losses"] = Changes(journal->losses);
        out["history"] = Changes(journal->history);
        return out;
    }
}
