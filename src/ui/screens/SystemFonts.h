#pragma once

#include <vector>

namespace romantasy::ui::theme
{
    // Windows' own fonts, used to draw the letters Romantasy's bundled fonts
    // lack. The bundled fonts are Latin subsets, so Cyrillic, Greek, Latin
    // Extended (Polish, Czech, Turkish...) and Chinese, Japanese and Korean
    // would otherwise show as '?'.
    //
    // Returns the fonts to merge behind one bundled font, in order: `weightFile`
    // (a Segoe UI file close to the bundled font's weight, e.g. L"seguisb.ttf"),
    // falling back to Segoe UI Regular, then Chinese, Japanese and Korean fonts
    // ordered by Windows' display language. Missing fonts are skipped. Each file
    // is read once for the whole process and shared, and the data never moves or
    // frees: the atlas reads glyphs from it as they are first drawn.
    [[nodiscard]] std::vector<const std::vector<unsigned char>*> FallbackFonts(const wchar_t* weightFile);
}
