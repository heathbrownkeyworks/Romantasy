set_xmakever('3.0.1')
includes('lib/commonlibsse-ng')

set_project('Romantasy')
set_version('2.0.1')
set_license('MIT')

set_languages('c++23')
set_warnings('allextra')
set_policy('package.requires_lock', true)
add_requires('nlohmann_json')
set_toolset('msvc', 'ninja')

add_rules('mode.debug', 'mode.releasedbg', 'mode.release')

-- CommonLib defaults skyrim_se, skyrim_ae, and skyrim_vr to true, so one DLL
-- contains all three layouts. Romantasy's supported UI runtimes remain SE + AE
-- because Meridian UI does not currently support VR.

target('Romantasy')
    add_deps('commonlibsse-ng')
    add_packages('nlohmann_json')

    add_rules('commonlibsse-ng.plugin', {
        name        = 'Romantasy',
        author      = 'ColdSun',
        description = 'Romance points tracker with a Meridian UI dashboard for Skyrim SE/AE.',
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
