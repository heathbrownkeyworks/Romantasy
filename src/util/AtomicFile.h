#pragma once

#include <filesystem>
#include <string_view>

namespace romantasy::files
{
    // Writes `text` to "<target>.tmp" in the same folder, closes it, then moves it over `target`.
    // On one volume the move is a rename. When the two paths resolve to different volumes (MO2's
    // virtual file system can do that) Windows copies the file, then deletes the temporary one.
    // Any failure removes the temporary file and returns false; `target` is left as it was unless
    // a cross-volume copy failed part way.
    [[nodiscard]] bool WriteTextAtomically(const std::filesystem::path& target, std::string_view text);
}
