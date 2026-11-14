#include "check.h"
#include "profiles/Lint.h"
#include "romance/RomanceProfiles.h"

#include <algorithm>
#include <string>

using namespace romantasy;
using romantasy::profilecheck::Finding;
using romantasy::profilecheck::Lint;

namespace
{
    using Level = Finding::Level;

    // A file with no findings at all.
    const std::string clean = R"(schema_version = 2

[npc]
plugin = "Fixture.esp"
form_id = "0x000801"

[romance]
likes = ["Quests Completed"]
favorites = ["Dungeons Cleared"]
dislikes = ["Items Stolen"]
pet_peeves = ["Murders"]
)";

    const std::string allTiers = R"(
[unlocks]
Acquaintance = ["a"]
Friend = ["b"]
Confidant = ["c"]
Lover = ["d"]
Spouse = ["e"]
)";

    std::size_t Count(const std::vector<Finding>& findings, Level level)
    {
        return static_cast<std::size_t>(std::ranges::count_if(findings, [&](const Finding& f) { return f.level == level; }));
    }

    const Finding* Find(const std::vector<Finding>& findings, Level level, std::string_view message)
    {
        for (const auto& f : findings) {
            if (f.level == level && f.message == message) return &f;
        }
        return nullptr;
    }

    bool HasPrefix(const std::vector<Finding>& findings, Level level, std::string_view prefix)
    {
        return std::ranges::any_of(findings, [&](const Finding& f) {
            return f.level == level && std::string_view(f.message).starts_with(prefix);
        });
    }

    std::string Replace(std::string text, std::string_view from, std::string_view to)
    {
        text.replace(text.find(from), from.size(), to);
        return text;
    }

    std::string Utf8Line(std::size_t codePoints)
    {
        std::string line;
        for (std::size_t i = 0; i < codePoints; ++i) line += "\xC3\xA9";  // e acute: 1 code point, 2 bytes
        return line;
    }
}

TEST(check_clean_file_has_no_findings)
{
    CHECK(Lint(clean, "t.toml").empty());
    CHECK(Lint(clean + allTiers, "t.toml").empty());
}

TEST(check_no_opinions_warns_on_the_romance_table)
{
    const auto text = Replace(clean, "likes = [\"Quests Completed\"]\nfavorites = [\"Dungeons Cleared\"]\ndislikes = [\"Items Stolen\"]\npet_peeves = [\"Murders\"]\n", "starting_level = \"Friend\"\n");
    const auto findings = Lint(text, "t.toml");
    CHECK(findings.size() == 1);
    const auto* f = Find(findings, Level::Warning, "no likes, dislikes, favorites or pet peeves: this follower reacts to nothing");
    CHECK(f != nullptr);
    CHECK(f && f->line == 7);
    // Empty lists count as nothing too; one filled list is enough.
    CHECK(Lint(Replace(clean, "likes = [\"Quests Completed\"]\nfavorites = [\"Dungeons Cleared\"]\ndislikes = [\"Items Stolen\"]\npet_peeves = [\"Murders\"]\n", "likes = []\ndislikes = []\n"), "t.toml").size() == 1);
    CHECK(Lint(Replace(clean, "likes = [\"Quests Completed\"]\nfavorites = [\"Dungeons Cleared\"]\ndislikes = [\"Items Stolen\"]\npet_peeves = [\"Murders\"]\n", "pet_peeves = [\"Murders\"]\n"), "t.toml").empty());
}

TEST(check_unknown_key_suggests_the_nearest_known_key)
{
    const auto findings = Lint(Replace(clean, "dislikes", "dislikess"), "t.toml");
    // The real parser rejects the key too, so there is one error plus the warning with the suggestion.
    CHECK(Count(findings, Level::Error) == 1);
    CHECK(Find(findings, Level::Error, "unknown field 'dislikess'") != nullptr);
    const auto* w = Find(findings, Level::Warning, "unknown key 'dislikess' (did you mean 'dislikes'?)");
    CHECK(w != nullptr);
    CHECK(w && w->line == 10);
    CHECK(findings.front().level == Level::Error && findings.front().line == 10);  // error before warning on a shared line
    CHECK(Find(Lint(Replace(clean, "likes", "like"), "t.toml"), Level::Warning, "unknown key 'like' (did you mean 'likes'?)") != nullptr);
    CHECK(Find(Lint(Replace(clean, "[npc]", "[npc]\nplugn = \"x.esp\""), "t.toml"), Level::Warning, "unknown key 'plugn' (did you mean 'plugin'?)") != nullptr);
    CHECK(Find(Lint(Replace(clean, "[npc]", "[npcs]"), "t.toml"), Level::Warning, "unknown key 'npcs' (did you mean 'npc'?)") != nullptr);
}

TEST(check_unknown_key_is_case_sensitive_and_stable)
{
    // 'Likes' is one edit from 'likes'.
    CHECK(Find(Lint(Replace(clean, "likes = [\"Q", "Likes = [\"Q"), "t.toml"), Level::Warning, "unknown key 'Likes' (did you mean 'likes'?)") != nullptr);
    // No suggestion past distance 2.
    const auto far = Lint(Replace(clean, "[npc]", "[npc]\nzzzzzz = 1"), "t.toml");
    CHECK(Find(far, Level::Warning, "unknown key 'zzzzzz'") != nullptr);
    // 'dislike' is one from 'dislikes' and two from 'likes': the nearest wins.
    CHECK(Find(Lint(Replace(clean, "dislikes = [", "dislike = ["), "t.toml"), Level::Warning, "unknown key 'dislike' (did you mean 'dislikes'?)") != nullptr);
    CHECK(Find(Lint(Replace(clean, "[npc]", "[npc]\nplugim = 1"), "t.toml"), Level::Warning, "unknown key 'plugim' (did you mean 'plugin'?)") != nullptr);
    // In a v1 file the v2-only keys are unknown.
    const auto v1 = Lint(Replace(clean, "schema_version = 2", "schema_version = 1"), "t.toml");
    CHECK(HasPrefix(v1, Level::Warning, "unknown key 'favorites'"));
}

TEST(check_unknown_key_names_in_unlocks)
{
    const auto findings = Lint(clean + "\n[unlocks]\nFrend = [\"x\"]\n", "t.toml");
    CHECK(Find(findings, Level::Warning, "unknown key 'Frend' (did you mean 'Friend'?)") != nullptr);
    CHECK(Count(findings, Level::Error) == 1);
}

TEST(check_every_key_the_lint_knows_is_accepted_by_the_parser)
{
    const std::string player = R"(schema_version = 2
[npc]
plugin = "X.esp"
form_id = "0x000801"
[romance]
starting_level = "Friend"
likes = ["Quests Completed"]
dislikes = ["Items Stolen"]
favorites = ["Dungeons Cleared"]
pet_peeves = ["Murders"]
enabled = true
)";
    CHECK(Lint(player, "t.toml", true).empty());
    const std::string mod = R"(schema_version = 2
[npc]
plugin = "X.esp"
form_id = "0x000801"
[romance]
starting_level = "Friend"
likes = ["Quests Completed"]
dislikes = ["Items Stolen"]
favorites = ["Dungeons Cleared"]
pet_peeves = ["Murders"]
[unlocks]
Acquaintance = ["a"]
Friend = ["b"]
Confidant = ["c"]
Lover = ["d"]
Spouse = ["e"]
)";
    CHECK(Lint(mod, "t.toml").empty());
    // 'enabled' is only known in a player profile.
    CHECK(Lint(player, "t.toml", false).size() == 2);
}

TEST(check_unlocks_for_only_some_tiers_are_fine)
{
    CHECK(Lint(clean + "\n[unlocks]\nFriend = [\"b\"]\nLover = [\"d\"]\n", "t.toml").empty());
    CHECK(Lint(clean + "\n[unlocks]\nSpouse = [\"e\"]\n", "t.toml").empty());
    CHECK(Lint(clean, "t.toml").empty());
    CHECK(Lint(clean + allTiers, "t.toml").empty());
}

TEST(check_unlock_line_width_counts_characters_not_bytes)
{
    const auto table = [](std::size_t n) {
        return clean + "\n[unlocks]\nAcquaintance = [\"" + Utf8Line(n) + "\"]\nFriend = [\"b\"]\nConfidant = [\"c\"]\nLover = [\"d\"]\nSpouse = [\"e\"]\n";
    };
    CHECK(Lint(table(48), "t.toml").empty());  // 96 bytes, 48 characters: fits
    const auto over = Lint(table(49), "t.toml");
    CHECK(over.size() == 1);
    const auto* f = Find(over, Level::Warning, "unlock line for Acquaintance is 49 characters and will wrap on the popup (48 fit on one line)");
    CHECK(f != nullptr);
    CHECK(f && f->line == 14);
    // 60 is the parser's limit: still a warning only. 61 is also an error from the parser.
    CHECK(Count(Lint(table(60), "t.toml"), Level::Error) == 0);
    const auto tooLong = Lint(table(61), "t.toml");
    CHECK(Count(tooLong, Level::Error) == 1);
    CHECK(Find(tooLong, Level::Warning, "unlock line for Acquaintance is 61 characters and will wrap on the popup (48 fit on one line)") != nullptr);
}

TEST(check_plugin_ending_warning)
{
    const auto findings = Lint(Replace(clean, "Fixture.esp", "Fixture"), "t.toml");
    CHECK(Find(findings, Level::Warning, "plugin 'Fixture' has no .esp, .esm or .esl ending") != nullptr);
    CHECK(Count(findings, Level::Error) == 1);  // the parser rejects it as well
    CHECK(Lint(Replace(clean, "Fixture.esp", "Fixture.ESM"), "t.toml").empty());
    CHECK(Lint(Replace(clean, "Fixture.esp", "Fixture.esl"), "t.toml").empty());
}

TEST(check_reports_the_parser_errors_unchanged)
{
    const auto bad = Lint(Replace(clean, "Quests Completed", "Quest Finished"), "some.toml");
    CHECK(bad.size() == 1);
    CHECK(bad[0].level == Level::Error);
    CHECK(bad[0].message == "unknown statistic 'Quest Finished'");  // no file name prefix; the file is a separate field
    CHECK(bad[0].line == 0);

    const auto syntax = Lint("schema_version = 1\n[npc\n", "some.toml");
    CHECK(syntax.size() == 1);
    CHECK(syntax[0].level == Level::Error);
    CHECK(syntax[0].line == 2);
    CHECK(!syntax[0].message.empty());

    CHECK(Count(Lint("", "t.toml"), Level::Error) == 1);
}

TEST(check_accepts_bom_and_crlf)
{
    CHECK(Lint("\xEF\xBB\xBF" + clean, "t.toml").empty());
    std::string crlf;
    for (const char c : clean) {
        if (c == '\n') crlf += '\r';
        crlf += c;
    }
    CHECK(Lint(crlf, "t.toml").empty());
    CHECK(Lint("\xEF\xBB\xBF" + crlf, "t.toml").empty());
}

TEST(check_invalid_utf8_is_an_error_naming_the_line)
{
    const auto text = Replace(clean, "[romance]\n", "[romance]\n# bad \xFF byte\n");
    const auto findings = Lint(text, "t.toml");
    CHECK(Count(findings, Level::Error) == 1);
    CHECK(Count(findings, Level::Warning) == 0);
    CHECK(findings.front().line == 8);
    // And inside a string value, and as the very first byte.
    const auto inString = Lint(Replace(clean, "Fixture.esp", "Fix\xC3.esp"), "t.toml");
    CHECK(Count(inString, Level::Error) == 1);
    CHECK(inString.front().line == 4);
    CHECK(Count(Lint("\xFF" + clean, "t.toml"), Level::Error) == 1);
}

TEST(check_findings_are_sorted_by_line_then_errors_first)
{
    const auto text = Replace(Replace(clean, "likes = [\"Quests Completed\"]", "likess = [\"Quests Completed\"]"), "Fixture.esp", "Fixture");
    const auto findings = Lint(text, "t.toml");
    CHECK(findings.size() >= 3);
    for (std::size_t i = 1; i < findings.size(); ++i) {
        const auto& a = findings[i - 1];
        const auto& b = findings[i];
        CHECK(a.line < b.line || (a.line == b.line && a.level <= b.level));
    }
}

namespace
{
    std::string WithMoments(const std::string& moments)
    {
        return Replace(clean, "schema_version = 2", "schema_version = 3") + "\n[moments]\n" + moments;
    }
}

TEST(check_moments_clean_file_has_no_findings)
{
    CHECK(Lint(WithMoments("affection = \"Acquaintance\"\ncloseness = \"Friend\"\nintimacy = \"never\"\nfeeling = \"treasured\"\n"), "t.toml").empty());
    CHECK(Lint(WithMoments("affection = \"Friend\"\ncloseness = \"Friend\"\nintimacy = \"Friend\"\n"), "t.toml").empty());
    CHECK(Lint(WithMoments(""), "t.toml").empty());
    CHECK(Lint(WithMoments("feeling = \"reserved\"\n") + allTiers, "t.toml").empty());
}

TEST(check_moments_unknown_key_suggests_the_nearest)
{
    const auto findings = Lint(WithMoments("closenes = \"Friend\"\n"), "t.toml");
    CHECK(Count(findings, Level::Error) == 1);
    const auto* e = Find(findings, Level::Error, "unknown field 'closenes'");
    CHECK(e != nullptr);
    CHECK(e && e->line == 14);
    const auto* w = Find(findings, Level::Warning, "unknown key 'closenes' (did you mean 'closeness'?)");
    CHECK(w != nullptr);
    CHECK(w && w->line == 14);
    CHECK(Find(Lint(WithMoments("feelin = \"warm\"\n"), "t.toml"), Level::Warning, "unknown key 'feelin' (did you mean 'feeling'?)") != nullptr);
}

TEST(check_moments_bad_tier_is_the_parser_error)
{
    const auto findings = Lint(WithMoments("affection = \"Freind\"\n"), "t.toml");
    CHECK(findings.size() == 1);
    CHECK(Find(findings, Level::Error, "unknown tier 'Freind' in [moments]") != nullptr);
}

TEST(check_moments_out_of_order_warns_for_each_pair)
{
    const auto findings = Lint(WithMoments("affection = \"Lover\"\ncloseness = \"Friend\"\nintimacy = \"Acquaintance\"\n"), "t.toml");
    CHECK(findings.size() == 3);
    CHECK(Count(findings, Level::Error) == 0);
    const auto* closeness = Find(findings, Level::Warning, "moments: closeness is welcome before affection");
    CHECK(closeness && closeness->line == 15);
    const auto* intimacyCloseness = Find(findings, Level::Warning, "moments: intimacy is welcome before closeness");
    CHECK(intimacyCloseness && intimacyCloseness->line == 16);
    const auto* intimacyAffection = Find(findings, Level::Warning, "moments: intimacy is welcome before affection");
    CHECK(intimacyAffection && intimacyAffection->line == 16);
    // Defaults fill a key that is left out.
    CHECK(Find(Lint(WithMoments("closeness = \"Acquaintance\"\n"), "t.toml"), Level::Warning, "moments: closeness is welcome before affection") != nullptr);
    CHECK(Find(Lint(WithMoments("affection = \"Spouse\"\n"), "t.toml"), Level::Warning, "moments: intimacy is welcome before affection") != nullptr);
}

TEST(check_moments_order_ignores_never)
{
    CHECK(Lint(WithMoments("affection = \"never\"\ncloseness = \"Friend\"\nintimacy = \"Acquaintance\"\n"), "t.toml").size() == 1);
    CHECK(Lint(WithMoments("affection = \"Lover\"\ncloseness = \"never\"\nintimacy = \"never\"\n"), "t.toml").empty());
    CHECK(Lint(WithMoments("affection = \"never\"\ncloseness = \"never\"\nintimacy = \"Stranger\"\n"), "t.toml").empty());
}

TEST(check_moments_in_older_or_player_files_is_refused)
{
    const auto v2 = Lint(clean + "\n[moments]\naffection = \"Friend\"\n", "t.toml");
    CHECK(Count(v2, Level::Error) == 1);
    CHECK(Find(v2, Level::Error, "unknown field 'moments'") != nullptr);
    CHECK(Find(v2, Level::Warning, "unknown key 'moments'") != nullptr);
    const auto player = Lint(WithMoments("affection = \"Friend\"\n"), "t.toml", true);
    CHECK(Find(player, Level::Error, "[moments] is not allowed in a player profile") != nullptr);
}

TEST(check_never_crashes_on_odd_shapes)
{
    // Wrong types where the parser throws and the warning pass must stay quiet and safe.
    CHECK(Count(Lint("npc = 1\nromance = 2\nunlocks = 3\nschema_version = 2\n", "t.toml"), Level::Error) == 1);
    CHECK(Count(Lint("schema_version = 2\n[npc]\nplugin = 5\nform_id = \"0x1\"\n[romance]\nlikes = 4\n[unlocks]\nFriend = 1\n", "t.toml"), Level::Error) == 1);
    CHECK(!Lint(std::string(100, '['), "t.toml").empty());
    CHECK(!Lint(std::string("a\0b", 3), "t.toml").empty());
}
