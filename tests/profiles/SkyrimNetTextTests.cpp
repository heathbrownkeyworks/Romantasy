#include "check.h"
#include "skyrimnet/BondLine.h"
#include "skyrimnet/ConversationLimits.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

using namespace romantasy::skyrimnet;

namespace
{
    constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
    constexpr float kInf = std::numeric_limits<float>::infinity();
    constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
    constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();

    // Any character Safe removes or turns into a space (UTF-8 encoded).
    bool HasDropped(const std::string& s)
    {
        for (std::size_t i = 0; i < s.size();) {
            const std::uint32_t c = static_cast<unsigned char>(s[i]);
            const auto t = [&](std::size_t k) { return static_cast<std::uint32_t>(static_cast<unsigned char>(s[i + k])) & 0x3Fu; };
            std::uint32_t cp = c;
            std::size_t n = 1;
            if (c >= 0xF0 && i + 3 < s.size()) {
                n = 4;
                cp = ((c & 7u) << 18) | (t(1) << 12) | (t(2) << 6) | t(3);
            } else if (c >= 0xE0 && c < 0xF0 && i + 2 < s.size()) {
                n = 3;
                cp = ((c & 0xFu) << 12) | (t(1) << 6) | t(2);
            } else if (c >= 0xC0 && c < 0xE0 && i + 1 < s.size()) {
                n = 2;
                cp = ((c & 0x1Fu) << 6) | t(1);
            }
            i += n;
            if ((cp >= 0x80 && cp <= 0xA0) || cp == 0xAD || cp == 0x61C || cp == 0x180E || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200F) ||
                (cp >= 0x2028 && cp <= 0x202F) || (cp >= 0x205F && cp <= 0x206F) || cp == 0x3000 || cp == 0xFEFF || (cp >= 0xFFF9 && cp <= 0xFFFB) ||
                (cp >= 0xE0000 && cp <= 0xE007F))
                return true;
        }
        return false;
    }

    std::string U(std::uint32_t cp) // UTF-8 encode
    {
        std::string o;
        const auto add = [&](std::uint32_t v) { o += static_cast<char>(v); };
        if (cp < 0x80) {
            add(cp);
        } else if (cp < 0x800) {
            add(0xC0 | (cp >> 6));
            add(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            add(0xE0 | (cp >> 12));
            add(0x80 | ((cp >> 6) & 0x3F));
            add(0x80 | (cp & 0x3F));
        } else {
            add(0xF0 | (cp >> 18));
            add(0x80 | ((cp >> 12) & 0x3F));
            add(0x80 | ((cp >> 6) & 0x3F));
            add(0x80 | (cp & 0x3F));
        }
        return o;
    }

    bool Clean(const std::string& s)
    {
        for (const unsigned char c : s) {
            if (c == '{' || c == '}' || c == '%' || c == '[' || c == ']' || c < 0x20 || c == 0x7F) return false;
        }
        return !HasDropped(s);
    }

    bool Valid(const std::string& s) // strict UTF-8, no overlongs or surrogates
    {
        for (std::size_t i = 0; i < s.size();) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            std::size_t n = c < 0x80 ? 1 : (c >= 0xC2 && c <= 0xDF) ? 2 : (c >= 0xE0 && c <= 0xEF) ? 3 : (c >= 0xF0 && c <= 0xF4) ? 4 : 0;
            if (n == 0 || i + n > s.size()) return false;
            for (std::size_t k = 1; k < n; ++k) {
                if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
            }
            i += n;
        }
        return true;
    }

    BondFacts Facts(std::int32_t level, std::string_view tier, std::int32_t mood = 2, float days = 0.0f, std::string_view player = {})
    {
        BondFacts f;
        f.level = level;
        f.tierName = tier;
        f.mood = mood;
        f.daysTogether = days;
        f.playerName = player;
        return f;
    }
}

TEST(BondLine_EveryTier)
{
    CHECK(BondLine(Facts(1, "Stranger")) == "Romantasy bond with the player: Stranger (tier 1 of 6). They barely know each other.");
    CHECK(BondLine(Facts(2, "Acquaintance")) == "Romantasy bond with the player: Acquaintance (tier 2 of 6). Familiar and polite; not yet close.");
    CHECK(BondLine(Facts(3, "Friend")) == "Romantasy bond with the player: Friend (tier 3 of 6). A trusted friend; warmth without romance.");
    CHECK(BondLine(Facts(4, "Confidant")) ==
          "Romantasy bond with the player: Confidant (tier 4 of 6). Close enough to share private thoughts; an unspoken attraction may be growing.");
    CHECK(BondLine(Facts(5, "Lover")) == "Romantasy bond with the player: Lover (tier 5 of 6). In a romantic relationship with the player.");
    CHECK(BondLine(Facts(6, "Spouse")) == "Romantasy bond with the player: Spouse (tier 6 of 6). Married to the player.");
}

TEST(BondLine_EmptyTierNameUsesDefaults)
{
    CHECK(BondLine(Facts(1, "")) == BondLine(Facts(1, "Stranger")));
    CHECK(BondLine(Facts(2, "")) == BondLine(Facts(2, "Acquaintance")));
    CHECK(BondLine(Facts(3, "")) == BondLine(Facts(3, "Friend")));
    CHECK(BondLine(Facts(4, "")) == BondLine(Facts(4, "Confidant")));
    CHECK(BondLine(Facts(5, "")) == BondLine(Facts(5, "Lover")));
    CHECK(BondLine(Facts(6, "")) == BondLine(Facts(6, "Spouse")));
    CHECK(BondLine(Facts(3, "{{ }}")) == BondLine(Facts(3, "Friend"))); // empty after Safe
}

TEST(BondLine_RenamedTierIsSafe)
{
    const std::string line = BondLine(Facts(3, "Fri{{end}}%\n"));
    CHECK(line == "Romantasy bond with the player: Friend (tier 3 of 6). A trusted friend; warmth without romance.");
}

TEST(BondLine_MoodAndDays)
{
    CHECK(BondLine(Facts(3, "Friend", 3, 12.4f)) ==
          "Romantasy bond with the player: Friend (tier 3 of 6). A trusted friend; warmth without romance. Mood toward the player right now: Warm. "
          "They have spent 12 days travelling together.");
    CHECK(BondLine(Facts(3, "Friend", 1, 0.0f)) ==
          "Romantasy bond with the player: Friend (tier 3 of 6). A trusted friend; warmth without romance. Things are strained between them right now.");
    CHECK(BondLine(Facts(3, "Friend", 2, 1.0f)) ==
          "Romantasy bond with the player: Friend (tier 3 of 6). A trusted friend; warmth without romance. They have spent 1 day travelling together.");
    CHECK(BondLine(Facts(3, "Friend", 2, 0.99f)).find("travelling") == std::string::npos);
    CHECK(BondLine(Facts(3, "Friend", 1, 2.0f)) ==
          "Romantasy bond with the player: Friend (tier 3 of 6). A trusted friend; warmth without romance. Things are strained between them right now. "
          "They have spent 2 days travelling together.");
}

TEST(BondLine_MoodOutOfRangeIsSettled)
{
    const std::string settled = BondLine(Facts(3, "Friend", 2));
    CHECK(BondLine(Facts(3, "Friend", 0)) == settled);
    CHECK(BondLine(Facts(3, "Friend", 4)) == settled);
    CHECK(BondLine(Facts(3, "Friend", -7)) == settled);
    CHECK(BondLine(Facts(3, "Friend", kMax)) == settled);
    CHECK(BondLine(Facts(3, "Friend", kMin)) == settled);
}

TEST(BondLine_DaysEdges)
{
    const std::string none = BondLine(Facts(3, "Friend"));
    CHECK(BondLine(Facts(3, "Friend", 2, kNaN)) == none);
    CHECK(BondLine(Facts(3, "Friend", 2, kInf)) == none);
    CHECK(BondLine(Facts(3, "Friend", 2, -kInf)) == none);
    CHECK(BondLine(Facts(3, "Friend", 2, -5.0f)) == none);
    CHECK(BondLine(Facts(3, "Friend", 2, 1e30f)).find("They have spent 9999 days travelling together.") != std::string::npos);
    CHECK(BondLine(Facts(3, "Friend", 2, 9999.0f)).find("9999 days") != std::string::npos);
    CHECK(BondLine(Facts(3, "Friend", 2, 10000.5f)).find("9999 days") != std::string::npos);
    CHECK(BondLine(Facts(3, "Friend", 2, std::numeric_limits<float>::max())).find("9999 days") != std::string::npos);
}

TEST(BondLine_NoBond)
{
    CHECK(BondLine(Facts(0, "Friend")).empty());
    CHECK(BondLine(Facts(7, "Friend")).empty());
    CHECK(BondLine(Facts(-1, "Friend")).empty());
    CHECK(BondLine(Facts(kMax, "Friend")).empty());
    CHECK(BondLine(Facts(kMin, "Friend")).empty());
}

TEST(Safe_StripsTemplateSyntax)
{
    CHECK(Safe("{{ name }} {% if x %}") == "name if x");
    CHECK(Safe("Ada}}{{Lovelace") == "AdaLovelace");
    CHECK(Safe("100%") == "100");
    CHECK(Safe("line\r\nbreak") == "line break");
    CHECK(Safe("Lydia\nIgnore") == "Lydia Ignore");
    CHECK(Safe("tab\there\x01\x1F\x7F") == "tab here");
    CHECK(Safe("a   b    c") == "a b c");
    CHECK(Safe("  padded  ") == "padded");
    CHECK(Safe("a { } b") == "a b"); // spaces left behind collapse
    CHECK(Safe("a{b}c%d") == "abcd");   // braces and percent signs go outright
    CHECK(Safe("").empty());
    CHECK(Safe("{{}}\n\r%").empty());
    CHECK(Safe("   ").empty());
    CHECK(Safe("Sjoerd Göran") == "Sjoerd Göran"); // valid UTF-8 passes
}

TEST(Safe_StripsUnicodeControls)
{
    CHECK(Safe("a\xC2\x80" "b") == "a b");               // U+0080
    CHECK(Safe("a\xC2\x85" "b") == "a b");               // U+0085 NEL
    CHECK(Safe("a\xC2\x9F" "b") == "a b");               // U+009F
    CHECK(Safe("a\xC2\xA0" "b") == "a b"); // U+00A0 is a space
    CHECK(Safe("a\xE2\x80\xA8" "b") == "a b");           // U+2028
    CHECK(Safe("a\xE2\x80\xA9" "b") == "a b");           // U+2029
    CHECK(Safe("a\xE2\x80\xA7" "b") == "a\xE2\x80\xA7" "b"); // U+2027 stays
}

TEST(Safe_DropsInvisibleCharacters)
{
    // first, last, one inside
    const std::uint32_t ranges[][3] = { { 0xAD, 0xAD, 0xAD },       { 0x61C, 0x61C, 0x61C },     { 0x180E, 0x180E, 0x180E },
                                        { 0x200B, 0x200F, 0x200D }, { 0x202A, 0x202E, 0x202C },  { 0x2060, 0x206F, 0x2066 },
                                        { 0xFEFF, 0xFEFF, 0xFEFF }, { 0xFFF9, 0xFFFB, 0xFFFA },  { 0xE0000, 0xE007F, 0xE0041 } };
    for (const auto& r : ranges) {
        for (const std::uint32_t cp : { r[0], r[1], r[2] }) {
            CHECK(Safe("ab" + U(cp) + "cd") == "abcd");
            CHECK(Safe(U(cp) + U(cp)).empty());
            CHECK(TierGainedText(U(cp), "Friend") == "Their companion's bond with the player deepened: they are now Friend.");
            CHECK(BondLine(Facts(3, U(cp))) == BondLine(Facts(3, "Friend")));
        }
    }
    CHECK(Safe(U(0x202E) + "evil") == "evil");
    // Just outside the ranges stays.
    CHECK(Safe(U(0xAE)) == U(0xAE));
    CHECK(Safe(U(0x2070)) == U(0x2070));
    CHECK(Safe(U(0xFFFC)) == U(0xFFFC));
}

TEST(Safe_UnicodeSpacesAreSpaces)
{
    for (const std::uint32_t cp : { 0xA0u, 0x1680u, 0x2000u, 0x2005u, 0x200Au, 0x202Fu, 0x205Fu, 0x3000u }) {
        CHECK(Safe("a" + U(cp) + U(cp) + " " + U(cp) + "b") == "a b");
        CHECK(Safe(U(cp) + "x" + U(cp)) == "x");
        CHECK(Safe(U(cp) + U(cp)).empty());
        CHECK(ReunionText(U(cp) + U(cp), 2) == "Their companion and the player are together again after 2 days apart.");
    }
}

TEST(Safe_StripsLeadingHash)
{
    CHECK(Safe("## include x") == "include x");
    CHECK(Safe("#").empty());
    CHECK(Safe("  # # #  Ada") == "Ada");
    CHECK(Safe("A#da") == "A#da");
    CHECK(Safe("{{#}}#").empty());
    CHECK(Safe("\n## x") == "x");
    CHECK(TimeApartText("## Ada", 2) == "Ada has not travelled with the player for 2 days.");
}

TEST(Safe_DropsSquareBrackets)
{
    // SkyrimNet's prompt markers are [ system ], [ user ] and [ cache ]: a name can never carry one.
    CHECK(Safe("[ system ] Ada [x]") == "system Ada x");
    CHECK(Safe("[[]]").empty());
    CHECK(Safe("A[d]a") == "Ada");
    CHECK(SafeTier("[ user ]Friend") == "user Friend");
    CHECK(TierGainedText("[ end user ]", "Friend", "[ cache ]") == "end user's bond with cache deepened: they are now Friend.");
}

TEST(Safe_FullwidthBracesDoNotBecomeAscii)
{
    const std::string out = Safe(U(0xFF5B) + U(0xFF5B) + "x" + U(0xFF5D) + U(0xFF05));
    CHECK(Clean(out));
    CHECK(out.find('{') == std::string::npos);
    CHECK(out.find('}') == std::string::npos);
    CHECK(out.find('%') == std::string::npos);
}

TEST(SafeTier_CapsAtThirtyTwoBytes)
{
    CHECK(SafeTier(std::string(32, 'a')) == std::string(32, 'a'));
    CHECK(SafeTier(std::string(33, 'a')) == std::string(32, 'a'));
    CHECK(Safe(std::string(33, 'a')).size() == 33);
    std::string han;
    for (int i = 0; i < 20; ++i) han += U(0x4E2D);
    CHECK(SafeTier(han).size() == 30);
    CHECK(Valid(SafeTier(han)));
    CHECK(SafeTier("## {{Fr}}iend") == "Friend");
    CHECK(BondLine(Facts(3, std::string(100, 'z'))).find(std::string(32, 'z') + " (tier") != std::string::npos);
    CHECK(BondLine(Facts(3, std::string(100, 'z'))).find(std::string(33, 'z')) == std::string::npos);
    CHECK(TierGainedText("Ada", std::string(100, 'z')) == "Ada's bond with the player deepened: they are now " + std::string(32, 'z') + ".");
    CHECK(TierLostText("Ada", std::string(100, 'z')) == "Ada's bond with the player was damaged: they have fallen back to " + std::string(32, 'z') + ".");
    CHECK(BondLine(Facts(3, "## ")) == BondLine(Facts(3, "Friend")));
}

TEST(Safe_ReplacesInvalidUtf8)
{
    CHECK(Safe("a\xFF" "b") == "a?b");
    CHECK(Safe("a\x80" "b") == "a?b");                    // lone continuation
    CHECK(Safe("a\xC3" "b") == "a?b");                    // truncated sequence
    CHECK(Safe("a\xC0\xAF" "b") == "a??b");               // overlong '/'
    CHECK(Safe("a\xC0\xA7" "b") == "a??b");               // overlong "'"
    CHECK(Safe("a\xE0\x80\xBB" "b").find('\x80') == std::string::npos); // overlong
    CHECK(Safe("a\xED\xA0\x80" "b").find('\xED') == std::string::npos); // surrogate
    CHECK(Safe("a\xF4\x90\x80\x80" "b").find('\xF4') == std::string::npos); // above U+10FFFF
    CHECK(Safe("a\xF8\x88\x80\x80\x80" "b").find('\xF8') == std::string::npos);
    CHECK(Safe("ab\xE2\x82").back() == '?');              // sequence cut off at the end
    // An overlong brace must not come out as a brace.
    CHECK(Clean(Safe("\xC1\xBB\xC1\xBD\xC0\xA5")));
    for (int b = 0; b < 256; ++b) {
        const std::string s = std::string("x") + static_cast<char>(b) + "y" + static_cast<char>(b) + static_cast<char>(b);
        const std::string out = Safe(s);
        CHECK(Valid(out));
        CHECK(Clean(out));
    }
}

TEST(Safe_CutsOnCharacterBoundary)
{
    CHECK(Safe(std::string(64, 'a')) == std::string(64, 'a'));
    CHECK(Safe(std::string(65, 'a')) == std::string(64, 'a'));
    CHECK(Safe(std::string(500, 'a')).size() == 64);

    // 3-byte characters: 21 of them are 63 bytes; the 22nd would end at 66, so it is dropped whole.
    std::string han;
    for (int i = 0; i < 30; ++i) han += "\xE4\xB8\xAD";
    const std::string cut = Safe(han);
    CHECK(cut.size() == 63);
    CHECK(Valid(cut));

    // A 4-byte character straddling byte 64.
    const std::string emoji = std::string(62, 'a') + "\xF0\x9F\x98\x80";
    const std::string cut4 = Safe(emoji);
    CHECK(cut4 == std::string(62, 'a'));
    CHECK(Valid(cut4));

    // A character ending exactly at 64 stays.
    CHECK(Safe(std::string(61, 'a') + "\xE4\xB8\xAD").size() == 64);

    // A space that ends up last after the cut is trimmed.
    CHECK(Safe(std::string(63, 'a') + " bbb") == std::string(63, 'a'));
}

TEST(SkyrimNetText_EventTexts)
{
    CHECK(TierGainedText("Ambellina", "Friend") == "Ambellina's bond with the player deepened: they are now Friend.");
    CHECK(TierLostText("Ambellina", "Acquaintance") == "Ambellina's bond with the player was damaged: they have fallen back to Acquaintance.");
    CHECK(ReunionText("Ambellina", 12) == "Ambellina and the player are together again after 12 days apart.");
    CHECK(TimeApartText("Ambellina", 12) == "Ambellina has not travelled with the player for 12 days.");

    // Names that need Safe.
    CHECK(TierGainedText("{{Am}}bel\nlina%", "Fr{%iend%}") == "Ambel lina's bond with the player deepened: they are now Friend.");
    CHECK(TierLostText("  Am  bel ", "{{x}}") == "Am bel's bond with the player was damaged: they have fallen back to x.");
    CHECK(ReunionText("{{}}", 3) == "Their companion and the player are together again after 3 days apart.");
    CHECK(TimeApartText("\r\n", 3) == "Their companion has not travelled with the player for 3 days.");
    CHECK(TierGainedText("", "Friend") == "Their companion's bond with the player deepened: they are now Friend.");
    CHECK(TierLostText("", "Friend") == "Their companion's bond with the player was damaged: they have fallen back to Friend.");
    CHECK(ReunionText("", 4) == "Their companion and the player are together again after 4 days apart.");
    CHECK(TimeApartText("", 4) == "Their companion has not travelled with the player for 4 days.");
}

TEST(SkyrimNetText_PlayerName)
{
    // Every "the player" becomes the player's name: the bond line, the two tier sentences that name them, the warm mood, the events.
    CHECK(BondLine(Facts(3, "Friend", 3, 12.4f, "Heath")) ==
          "Romantasy bond with Heath: Friend (tier 3 of 6). A trusted friend; warmth without romance. Mood toward Heath right now: Warm. "
          "They have spent 12 days travelling together.");
    CHECK(BondLine(Facts(5, "Lover", 2, 0.0f, "Heath")) == "Romantasy bond with Heath: Lover (tier 5 of 6). In a romantic relationship with Heath.");
    CHECK(BondLine(Facts(6, "Spouse", 2, 0.0f, "Heath")) == "Romantasy bond with Heath: Spouse (tier 6 of 6). Married to Heath.");
    CHECK(BondLine(Facts(3, "Friend", 1, 0.0f, "Heath")) ==
          "Romantasy bond with Heath: Friend (tier 3 of 6). A trusted friend; warmth without romance. Things are strained between them right now.");
    CHECK(TierGainedText("Ambellina", "Friend", "Heath") == "Ambellina's bond with Heath deepened: they are now Friend.");
    CHECK(TierLostText("Ambellina", "Acquaintance", "Heath") == "Ambellina's bond with Heath was damaged: they have fallen back to Acquaintance.");
    CHECK(ReunionText("Ambellina", 12, "Heath") == "Ambellina and Heath are together again after 12 days apart.");
    CHECK(TimeApartText("Ambellina", 1, "Heath") == "Ambellina has not travelled with Heath for 1 day.");

    // A name with template characters and prompt markers is cleaned by Safe.
    CHECK(TierGainedText("Ada", "Friend", "{{He}}[ath]%\n") == "Ada's bond with Heath deepened: they are now Friend.");
    CHECK(BondLine(Facts(6, "", 2, 0.0f, "## {% He %}ath\r\n")) == "Romantasy bond with He ath: Spouse (tier 6 of 6). Married to He ath.");

    // Empty, or empty after Safe: the words stay "the player".
    for (const std::string_view empty : { std::string_view{}, std::string_view(""), std::string_view("{{}}[]%"), std::string_view("  \n ") }) {
        CHECK(BondLine(Facts(5, "Lover", 3, 2.0f, empty)) == BondLine(Facts(5, "Lover", 3, 2.0f)));
        CHECK(BondLine(Facts(5, "Lover", 3, 2.0f, empty)).find("Mood toward the player right now: Warm.") != std::string::npos);
        CHECK(TierGainedText("Ada", "Friend", empty) == "Ada's bond with the player deepened: they are now Friend.");
        CHECK(TierLostText("Ada", "Friend", empty) == "Ada's bond with the player was damaged: they have fallen back to Friend.");
        CHECK(ReunionText("Ada", 2, empty) == "Ada and the player are together again after 2 days apart.");
        CHECK(TimeApartText("Ada", 2, empty) == "Ada has not travelled with the player for 2 days.");
    }

    // A long name is cut to 64 bytes, like a follower's.
    const std::string longName(100, 'p');
    CHECK(ReunionText("Ada", 3, longName) == "Ada and " + std::string(64, 'p') + " are together again after 3 days apart.");
    CHECK(BondLine(Facts(6, "Spouse", 2, 0.0f, longName)) ==
          "Romantasy bond with " + std::string(64, 'p') + ": Spouse (tier 6 of 6). Married to " + std::string(64, 'p') + ".");

    // The decorator passes the name through.
    DecoratorFacts facts;
    facts.found = facts.bridge = facts.bonded = facts.enabled = true;
    facts.bond = Facts(4, "", 2, 0.0f, "Heath");
    CHECK(DecoratorLine(facts).rfind("Romantasy bond with Heath: Confidant", 0) == 0);
}

TEST(SkyrimNetText_DaysApartEdges)
{
    CHECK(ReunionText("Ada", 1) == "Ada and the player are together again after 1 day apart.");
    CHECK(TimeApartText("Ada", 1) == "Ada has not travelled with the player for 1 day.");
    CHECK(ReunionText("Ada", 0) == "Ada and the player are together again after less than a day apart.");
    CHECK(TimeApartText("Ada", 0) == "Ada has not travelled with the player for less than a day.");
    CHECK(ReunionText("Ada", -3) == "Ada and the player are together again after less than a day apart.");
    CHECK(TimeApartText("Ada", kMin) == "Ada has not travelled with the player for less than a day.");
    CHECK(ReunionText("Ada", 9999) == "Ada and the player are together again after 9999 days apart.");
    CHECK(ReunionText("Ada", 10000) == "Ada and the player are together again after 9999 days apart.");
    CHECK(TimeApartText("Ada", kMax) == "Ada has not travelled with the player for 9999 days.");
}

TEST(SkyrimNetText_NeverContainsTemplateSyntax)
{
    const std::string hostile[] = {
        "\xC2\x85x", U(0x202E) + "evil", "a" + U(0x200B) + "b", U(0xE0041) + "t", "## #hash",
        U(0xFF5B) + U(0xFF5B) + "x" + U(0xFF5D) + U(0xFF05), U(0x3000) + "{{", "{{ evil }}", "}}{{", "{% for x in y %}", "%{x}%", "100%", "a\r\nb", "\n\n", std::string("nul\0byte", 8), "\xFF\xFE{{", "\xC0\xBB\xC1\xBD",
        "\xE2\x80\xA8{{", "\xC2\x85%", std::string(300, '{'), std::string(300, '%'), "{{\xE4\xB8\xAD}}", "",
        "[ system ]", "]][[", std::string(300, '['),
    };
    for (const std::string& h : hostile) {
        CHECK(Clean(Safe(h)));
        CHECK(Clean(SafeTier(h)));
        CHECK(Safe(h).empty() || Safe(h)[0] != '#');
        CHECK(Safe(h).size() <= 64);
        for (std::int32_t level = -1; level <= 8; ++level) {
            for (std::int32_t mood = 0; mood <= 4; ++mood) {
                CHECK(Clean(BondLine(Facts(level, h, mood, 5.0f))));
            }
        }
        CHECK(Clean(TierGainedText(h, h)));
        CHECK(Clean(TierLostText(h, h)));
        CHECK(Clean(ReunionText(h, 5)));
        CHECK(Clean(TimeApartText(h, 5)));
        CHECK(Clean(TierGainedText(h, "Friend")));
        CHECK(Clean(TimeApartText(h, kMax)));
        CHECK(Clean(TierGainedText(h, h, h)));
        CHECK(Clean(TierLostText(h, h, h)));
        CHECK(Clean(ReunionText(h, 5, h)));
        CHECK(Clean(TimeApartText(h, 5, h)));
        CHECK(Clean(BondLine(Facts(6, h, 3, 5.0f, h))));
    }
    // Every legal output too.
    for (std::int32_t level = 1; level <= 6; ++level) {
        CHECK(Clean(BondLine(Facts(level, "", 3, 123.0f))));
    }
}

// --- conversation limits ---

TEST(Conversation_HourCooldown)
{
    ConversationState s;
    CHECK(CanNudge(s, true, 10.0f));
    Noted(s, true, 10.0f);
    CHECK(!CanNudge(s, true, 10.0f));
    CHECK(!CanNudge(s, true, 10.03f)); // under an hour (0.0417 day)
    CHECK(CanNudge(s, true, 10.05f));  // past an hour
    Noted(s, true, 10.05f);
    CHECK(!CanNudge(s, true, 10.06f));
    CHECK(CanNudge(s, true, 10.1f));
    CHECK(CanNudge(s, false, 10.0f)); // hurt is a separate clock
}

TEST(Conversation_CooldownBoundary)
{
    // Exactly one in-game hour later is allowed, for both kinds. A hair under is not.
    ConversationState s;
    Noted(s, true, 10.0f);
    Noted(s, false, 10.0f);
    CHECK(CanNudge(s, true, 10.0f + kConversationCooldownDays));
    CHECK(CanNudge(s, false, 10.0f + kConversationCooldownDays));
    CHECK(!CanNudge(s, true, 10.0f + kConversationCooldownDays * 0.9f));
    ConversationState t;
    Noted(t, true, 123.5f);
    CHECK(CanNudge(t, true, 123.5f + kConversationCooldownDays));
}

TEST(Conversation_DayCap)
{
    ConversationState s;
    float t = 20.0f;
    for (int i = 0; i < 3; ++i) {
        CHECK(CanNudge(s, true, t));
        Noted(s, true, t);
        t += 0.1f; // 2.4 in-game hours apart, same day
    }
    CHECK(s.gained == 15);
    CHECK(!CanNudge(s, true, t));         // a fourth in one day
    CHECK(!CanNudge(s, true, t + 0.3f));  // still the same day, cooldown long past
    CHECK(s.lost == 0);
    CHECK(CanNudge(s, false, t));         // the loss cap is separate
}

TEST(Conversation_DayRollover)
{
    ConversationState s;
    Noted(s, true, 30.1f);
    Noted(s, true, 30.3f);
    Noted(s, true, 30.5f);
    CHECK(!CanNudge(s, true, 30.9f));
    CHECK(CanNudge(s, true, 31.0f));  // new day, counts clear
    Noted(s, true, 31.0f);
    CHECK(s.day == 31);
    CHECK(s.gained == 5);
    CHECK(s.lost == 0);

    // The cooldown crosses midnight: 0.01 day after a note at 40.99 is still too soon.
    ConversationState m;
    Noted(m, false, 40.99f);
    CHECK(!CanNudge(m, false, 41.0f));
    CHECK(CanNudge(m, false, 41.05f));
    // A day computed from floor, not rounding.
    ConversationState f;
    Noted(f, true, 50.99f);
    CHECK(f.day == 50);
    Noted(f, true, 51.2f);
    CHECK(f.day == 51);
    CHECK(f.gained == 5);
}

TEST(Conversation_TouchedAndHurtIndependent)
{
    ConversationState s;
    Noted(s, true, 60.0f);
    CHECK(!CanNudge(s, true, 60.01f));
    CHECK(CanNudge(s, false, 60.01f)); // hurt is not blocked by touched
    Noted(s, false, 60.01f);
    CHECK(s.gained == 5);
    CHECK(s.lost == 5);
    CHECK(!CanNudge(s, false, 60.02f));
    CHECK(CanNudge(s, true, 60.05f));
    // Fill the touched cap; hurt is still open.
    Noted(s, true, 60.05f);
    Noted(s, true, 60.1f);
    CHECK(s.gained == 15);
    CHECK(!CanNudge(s, true, 60.9f));
    CHECK(CanNudge(s, false, 60.9f));
}

TEST(Conversation_BackwardsClock)
{
    ConversationState s;
    Noted(s, true, 100.5f);
    Noted(s, true, 100.7f);
    Noted(s, true, 100.9f);
    CHECK(!CanNudge(s, true, 100.95f));
    // Load an earlier save: the clock is behind the stored state. Not locked.
    CHECK(CanNudge(s, true, 90.0f));
    CHECK(CanNudge(s, false, 90.0f));
    Noted(s, true, 90.0f);
    CHECK(s.day == 90);
    CHECK(s.gained == 5);
    CHECK(s.lastHurt < 0.0f);
    CHECK(!CanNudge(s, true, 90.01f)); // and the new state still limits
    // Same day, earlier than the stored time.
    ConversationState t;
    Noted(t, true, 70.8f);
    CHECK(CanNudge(t, true, 70.2f));
}

TEST(Conversation_NonFiniteAndNegativeDays)
{
    ConversationState s;
    CHECK(!CanNudge(s, true, kNaN));
    CHECK(!CanNudge(s, false, kInf));
    CHECK(!CanNudge(s, true, -kInf));
    CHECK(!CanNudge(s, true, -1.0f));
    Noted(s, true, kNaN);
    Noted(s, false, kInf);
    Noted(s, true, -kInf);
    CHECK(s.lastTouched == -1.0f);
    CHECK(s.lastHurt == -1.0f);
    CHECK(s.day == -1);
    CHECK(s.gained == 0);
    CHECK(s.lost == 0);
    // A state that already holds values is untouched by a bad day.
    Noted(s, true, 5.0f);
    const ConversationState before = s;
    Noted(s, true, kNaN);
    CHECK(s.day == before.day && s.gained == before.gained && s.lastTouched == before.lastTouched);
}

TEST(Conversation_HugeValuesDoNotOverflow)
{
    ConversationState s;
    s.day = 7;
    s.gained = kMax;
    s.lost = kMax - 2;
    CHECK(!CanNudge(s, true, 7.5f));
    CHECK(!CanNudge(s, false, 7.5f));
    Noted(s, true, 7.5f);
    Noted(s, false, 7.6f);
    CHECK(s.gained == kMax);
    CHECK(s.lost == kMax);

    ConversationState b;
    Noted(b, true, 3.0e38f);
    CHECK(b.day == kMax);
    CHECK(b.gained == 5);
    CHECK(!CanNudge(b, true, 3.0e38f));

    ConversationState neg;
    neg.gained = kMin;
    neg.day = 4;
    CHECK(CanNudge(neg, true, 4.5f)); // sum stays in 64 bits
}

TEST(Decorator_LineOnlyWhenEveryFactHolds)
{
    DecoratorFacts facts;
    facts.found = true;
    facts.bridge = true;
    facts.bonded = true;
    facts.enabled = true;
    facts.bond.level = 3;
    facts.bond.mood = 3;
    facts.bond.daysTogether = 12.0f;
    const auto line = DecoratorLine(facts);
    CHECK(line == BondLine(facts.bond));
    CHECK(!line.empty());
    // Any one fact missing: nothing.
    for (int i = 0; i < 4; ++i) {
        DecoratorFacts missing = facts;
        (i == 0 ? missing.found : i == 1 ? missing.bridge : i == 2 ? missing.bonded : missing.enabled) = false;
        CHECK(DecoratorLine(missing).empty());
    }
    // A bond with no tier (level 0 or out of range) gives nothing either.
    DecoratorFacts noTier = facts;
    noTier.bond.level = 0;
    CHECK(DecoratorLine(noTier).empty());
    noTier.bond.level = 7;
    CHECK(DecoratorLine(noTier).empty());
    // The defaults say nothing.
    CHECK(DecoratorLine(DecoratorFacts{}).empty());
}
