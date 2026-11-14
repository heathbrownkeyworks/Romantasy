set_xmakever('3.0.1')
includes('lib/commonlibsse-ng')

set_project('Romantasy')
set_version('4.0.0')
set_license('GPL-3.0-or-later')

set_languages('c++23')
set_warnings('allextra')
set_policy('package.requires_lock', true)
add_requires('nlohmann_json')
add_requires('toml++ v3.4.0')
set_toolset('msvc', 'ninja')

add_rules('mode.debug', 'mode.releasedbg', 'mode.release')

-- Name of the mod folder under XSE_TES5_MODS_PATH that the DLL auto-installs
-- into. Empty keeps CommonLib's default (the target name).
option('modfolder')
    set_default('')
    set_showmenu(true)
    set_description('Mod folder name under XSE_TES5_MODS_PATH to install the DLL into')
option_end()

-- The CommonLib rule auto-installs into "<XSE_TES5_MODS_PATH>/<target name>".
-- When the installed mod folder has a different name, persist it once with
--   xmake f --modfolder="<folder name>"
-- and the DLL lands there instead; scripts/deploy.ps1 then signs and verifies
-- the full payload in the same folder. Applied as a rule added after the
-- CommonLib rule so its config step runs last.
rule('romantasy.modfolder')
    on_config(function(target)
        local mods = os.getenv('XSE_TES5_MODS_PATH')
        local folder = get_config('modfolder')
        if mods and folder and folder ~= '' then
            target:set('installdir', path.join(mods, folder))
        end
    end)
rule_end()

target('Romantasy')
    add_deps('commonlibsse-ng')
    add_packages('nlohmann_json', 'toml++')
    add_defines('NOMINMAX')

    add_rules('commonlibsse-ng.plugin', {
        name        = 'Romantasy',
        author      = 'ColdSun',
        description = 'Romance points tracker with a Magelight UI dashboard for Skyrim SE/AE.',
        options = {
            address_library = true,
            struct_dependent = false
        }
    })

    add_files('src/**.cpp')
    add_headerfiles('src/**.h')

    add_includedirs(
        'src',
        '$(projectdir)'
    )

    set_pcxxheader('src/pch.h')
    add_rules('romantasy.modfolder')  -- after the CommonLib rule so its install folder wins

-- Skyrim-free tests for the profile loader and native condition protocol.
target('romantasy-profile-tests')
    set_kind('binary')
    set_default(false)
    set_rundir('$(projectdir)')
    add_packages('toml++')
    add_files('tests/profiles/*.cpp', 'src/romance/RomanceProfiles.cpp', 'src/romance/SaveCodec.cpp', 'src/romance/PointsView.cpp', 'src/romance/Journal.cpp', 'src/romance/Why.cpp', 'src/skyrimnet/BondLine.cpp', 'src/util/AtomicFile.cpp', 'tools/profiles/Lint.cpp')
    add_includedirs('src', 'tests', 'tools')
    add_defines('NOMINMAX', 'WIN32_LEAN_AND_MEAN')

-- Skyrim-free tests for the page payload parser and the popup wait policy.
target('romantasy-page-tests')
    set_kind('binary')
    set_default(false)
    set_rundir('$(projectdir)')
    add_packages('nlohmann_json')
    add_files('tests/page/*.cpp', 'src/ui/PageRequests.cpp', 'src/ui/PopupQueue.cpp', 'src/ui/Whispers.cpp', 'src/ui/JournalJson.cpp', 'src/romance/Journal.cpp', 'src/health/Health.cpp')
    add_includedirs('src', 'tests')
    add_defines('NOMINMAX', 'WIN32_LEAN_AND_MEAN')

target('romantasy-profile-check')
    set_kind('binary')
    set_default(false)
    add_packages('toml++', 'nlohmann_json')
    add_files('tools/profiles/main.cpp', 'tools/profiles/Lint.cpp', 'src/romance/RomanceProfiles.cpp', 'src/util/AtomicFile.cpp')
    add_includedirs('src', 'tools')
    add_defines('NOMINMAX', 'WIN32_LEAN_AND_MEAN')
