set_xmakever('3.0.1')
includes('lib/commonlibsse-ng')

set_project('Romantasy')
set_version('3.0.0')
set_license('GPL-3.0-or-later')

set_languages('c++23')
set_warnings('allextra')
set_policy('package.requires_lock', true)
add_requires('nlohmann_json')
add_requires('toml++ v3.4.0')
add_requires('imgui v1.92.6', {configs = {dx11 = true, win32 = true, freetype = true}})
add_requires('stb')
set_toolset('msvc', 'ninja')

add_rules('mode.debug', 'mode.releasedbg', 'mode.release')

-- CommonLib defaults skyrim_se, skyrim_ae, and skyrim_vr to true, so one DLL
-- contains all three layouts. The ImGui dashboard is SE + AE only (DX11 menus).

target('Romantasy')
    add_deps('commonlibsse-ng')
    add_packages('nlohmann_json', 'imgui', 'toml++')
    add_defines('NOMINMAX')  -- Windows.h reaches every TU through the PCH; keep std::min/std::max usable in src/ui/screens

    add_rules('commonlibsse-ng.plugin', {
        name        = 'Romantasy',
        author      = 'ColdSun',
        description = 'Romance points tracker with an in-game ImGui dashboard for Skyrim SE/AE.',
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

-- Skyrim-free unit tests for src/ui/screens. Build + run:
--   xmake build romantasy-ui-tests && xmake run romantasy-ui-tests
target('romantasy-ui-tests')
    set_kind('binary')
    set_default(false)
    set_rundir('$(projectdir)')
    add_packages('nlohmann_json', 'imgui')
    add_files('tests/ui/*.cpp', 'src/ui/screens/*.cpp')
    add_includedirs('src')
    add_defines('NOMINMAX', 'WIN32_LEAN_AND_MEAN')

-- Desktop preview of the ImGui screens. Build:
--   xmake build romantasy-preview
-- Run from the repo root (fonts and fixture paths are relative to it):
--   build\windows\x64\release\romantasy-preview.exe --shot build\preview.png
target('romantasy-preview')
    set_kind('binary')
    set_default(false)
    set_rundir('$(projectdir)')
    add_packages('nlohmann_json', 'imgui', 'stb')
    add_files('tools/preview/main.cpp', 'src/ui/screens/*.cpp')
    add_includedirs('src')
    add_defines('NOMINMAX', 'WIN32_LEAN_AND_MEAN')
    add_syslinks('d3d11', 'dxgi', 'd3dcompiler', 'user32', 'gdi32', 'shell32')

-- Skyrim-free tests for the profile loader and native condition protocol.
target('romantasy-profile-tests')
    set_kind('binary')
    set_default(false)
    set_rundir('$(projectdir)')
    add_packages('toml++')
    add_files('tests/profiles/*.cpp', 'src/romance/RomanceProfiles.cpp')
    add_includedirs('src', 'tests/ui')
    add_defines('NOMINMAX', 'WIN32_LEAN_AND_MEAN')

target('romantasy-profile-check')
    set_kind('binary')
    set_default(false)
    add_packages('toml++')
    add_files('tools/profiles/main.cpp', 'src/romance/RomanceProfiles.cpp')
    add_includedirs('src')
    add_defines('NOMINMAX', 'WIN32_LEAN_AND_MEAN')
