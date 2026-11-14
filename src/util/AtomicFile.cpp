#include "util/AtomicFile.h"

#include <fstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace romantasy::files
{
    namespace
    {
        // Replaces `target` with `temporary`; on Windows the move is flushed to disk before it returns.
        // Across volumes (possible under MO2's virtual file system) Windows copies, then deletes.
        bool Replace(const std::filesystem::path& temporary, const std::filesystem::path& target)
        {
#ifdef _WIN32
            return ::MoveFileExW(
                       temporary.c_str(),
                       target.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH) != 0;
#else
            std::error_code ec;
            std::filesystem::rename(temporary, target, ec);
            return !ec;
#endif
        }
    }

    bool WriteTextAtomically(const std::filesystem::path& target, std::string_view text)
    {
        std::filesystem::path temporary = target;
        temporary += ".tmp";

        bool written = false;
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (file) {
                file.write(text.data(), static_cast<std::streamsize>(text.size()));
                file.close();
                written = !file.fail();
            }
        }

        if (written && Replace(temporary, target)) {
            return true;
        }
        // Best effort, and only a file this call may have left: anything else at that path is not ours.
        std::error_code ec;
        if (std::filesystem::is_regular_file(temporary, ec)) {
            std::filesystem::remove(temporary, ec);
        }
        return false;
    }
}
