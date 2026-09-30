import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const commonLibDir = join(root, 'lib', 'commonlibsse-ng');
const expectedCommit = '7a60f4de794095d7b0f8928d1b930a52e9a7da83';

const git = (...args) => execFileSync('git', args, { encoding: 'utf8' }).trim();
const gitmodules = readFileSync(join(root, '.gitmodules'), 'utf8');
const project = readFileSync(join(root, 'xmake.lua'), 'utf8');
const commonLibBuild = readFileSync(join(commonLibDir, 'xmake.lua'), 'utf8');
const commonLibInterfaces = readFileSync(
    join(commonLibDir, 'include', 'SKSE', 'Interfaces.h'),
    'utf8'
);
const pluginTemplate = readFileSync(
    join(commonLibDir, 'res', 'commonlibsse-ng-plugin.cpp.in'),
    'utf8'
);

assert.match(
    gitmodules,
    /url\s*=\s*https:\/\/github\.com\/alandtse\/CommonLibSSE-NG\.git/,
    'Romantasy must use the maintained CommonLibSSE-NG repository'
);
assert.equal(
    git('-C', commonLibDir, 'rev-parse', 'HEAD'),
    expectedCommit,
    'Romantasy must pin the reviewed CommonLibSSE-NG 7.2.0 release commit'
);

for (const runtime of ['skyrim_se', 'skyrim_ae', 'skyrim_vr']) {
    const optionBlock = new RegExp(
        `option\\("${runtime}"[\\s\\S]*?set_default\\(true\\)[\\s\\S]*?end\\)`,
        'm'
    );
    assert.match(commonLibBuild, optionBlock, `${runtime} must remain enabled by default`);
}

assert.match(project, /address_library\s*=\s*true/);
assert.match(project, /struct_dependent\s*=\s*false/);
assert.match(
    commonLibInterfaces,
    /versionIndependenceEx\s*=\s*kVersionIndependentEx_AddressLibraryV5/,
    'CommonLib must emit Address Library v5 compatibility metadata'
);
assert.match(
    pluginTemplate,
    /StructCompatibility\s*=\s*\$\{COMMONLIBSSE_NG_OPTION_STRUCT_COMPATIBILITY\}/
);
assert.match(
    pluginTemplate,
    /RuntimeCompatibility\s*=\s*\$\{COMMONLIBSSE_NG_OPTION_RUNTIME_COMPATIBILITY\}/
);

console.log('Runtime compatibility contract tests passed.');
