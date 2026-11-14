// Runs the built romantasy-profile-check.exe on the fixtures under tests/fixtures/profiles
// and on the profiles shipped in examples/profiles. Build it first:
//   xmake build romantasy-profile-check
import { spawnSync } from 'node:child_process';
import assert from 'node:assert/strict';
import { copyFileSync, existsSync, mkdirSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const exe = join(root, 'build', 'windows', 'x64', 'release', 'romantasy-profile-check.exe');
const fixtures = join(root, 'tests', 'fixtures', 'profiles');
const fixture = (name) => join(fixtures, name);

if (!existsSync(exe)) {
    console.error(`romantasy-profile-check.exe is not built: expected ${exe}\nRun: xmake build romantasy-profile-check`);
    process.exit(1);
}

const run = (...args) => {
    const r = spawnSync(exe, args, { encoding: 'utf8' });
    assert.equal(r.error, undefined, `could not run the checker: ${r.error}`);
    return { code: r.status, out: r.stdout, err: r.stderr };
};
const lines = (text) => text.split('\n').filter(Boolean);
const findingLines = (out) => lines(out).filter((l) => /: (error|warning): /.test(l));

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const temp = mkdtempSync(join(tmpdir(), 'romantasy-check-'));

const NO_OPINIONS = 'no likes, dislikes, favorites or pet peeves: this follower reacts to nothing';

test('a clean file has no findings and exits 0, also under --strict', () => {
    for (const args of [[], ['--strict']]) {
        const r = run(...args, fixture('clean.toml'));
        assert.equal(r.code, 0, r.out + r.err);
        assert.deepEqual(findingLines(r.out), []);
        assert.match(r.out, /1 valid profile\(s\), 0 error\(s\), 0 warning\(s\)\./);
        assert.equal(r.err, '');
    }
});

test('no opinions: a warning, exit 0, exit 2 under --strict', () => {
    const file = fixture('no-opinions.toml');
    const r = run(file);
    assert.equal(r.code, 0);
    assert.deepEqual(findingLines(r.out), [`${file}(7): warning: ${NO_OPINIONS}`]);
    assert.equal(run('--strict', file).code, 2);
});

test('unlock lines for only some tiers are clean, also under --strict', () => {
    const r = run('--strict', fixture('unlock-partial.toml'));
    assert.equal(r.code, 0, r.out);
    assert.deepEqual(findingLines(r.out), []);
});

test('the shipped ExampleFollower.toml.example is clean under --strict once renamed', () => {
    const dir = join(temp, 'template');
    mkdirSync(dir);
    copyFileSync(join(root, 'examples', 'profiles', 'ExampleFollower.toml.example'), join(dir, 'ExampleFollower.toml'));
    const r = run('--strict', dir);
    assert.equal(r.code, 0, r.out + r.err);
    assert.deepEqual(findingLines(r.out), []);
});

test('a warning that hints at a parser error always follows that error', () => {
    const r = run(fixture('unknown-key.toml'));
    const found = findingLines(r.out);
    assert.equal(found.length, 2, r.out);
    assert.match(found[0], /\(8\): error: unknown field 'likess'$/);
    assert.match(found[1], /\(8\): warning: unknown key 'likess' \(did you mean 'likes'\?\)$/);
});

test('a long unlock line: a warning with the count', () => {
    const file = fixture('unlock-long.toml');
    const r = run(file);
    assert.equal(r.code, 0);
    const found = findingLines(r.out);
    assert.deepEqual(found, [`${file}(15): warning: unlock line for Friend is 55 characters and will wrap on the popup (48 fit on one line)`]);
    assert.equal(run('--strict', file).code, 2);
});

test('an unknown key: the parser error stays and the warning suggests a name', () => {
    const file = fixture('unknown-key.toml');
    const r = run(file);
    assert.equal(r.code, 1);
    assert.deepEqual(findingLines(r.out), [
        `${file}(8): error: unknown field 'likess'`,
        `${file}(8): warning: unknown key 'likess' (did you mean 'likes'?)`,
    ]);
});

test('a plugin without an ending: the parser error stays and a warning is added', () => {
    const file = fixture('plugin-ending.toml');
    const r = run(file);
    assert.equal(r.code, 1);
    const found = findingLines(r.out);
    assert.ok(found.includes(`${file}(4): warning: plugin 'Fixture' has no .esp, .esm or .esl ending`), r.out);
    assert.ok(found.some((l) => l.startsWith(`${file}: error: plugin must be`)), r.out);
});

test('a syntax error: error with its line, exit 1', () => {
    const file = fixture('broken.toml');
    const r = run(file);
    assert.equal(r.code, 1);
    const found = findingLines(r.out);
    assert.equal(found.length, 1, r.out);
    assert.match(found[0], new RegExp(`^${file.replace(/[\\^$.*+?()[\]{}|]/g, '\\$&')}\\(\\d+\\): error: `));
    assert.match(r.out, /0 valid profile\(s\), 1 error\(s\), 0 warning\(s\)\./);
});

test('a BOM and CRLF line ends are accepted silently', () => {
    for (const name of ['bom.toml', 'crlf.toml']) {
        const r = run('--strict', fixture(name));
        assert.equal(r.code, 0, `${name}: ${r.out}${r.err}`);
        assert.deepEqual(findingLines(r.out), []);
    }
});

test('an invalid UTF-8 byte is an error that names the line', () => {
    const file = fixture('bad-utf8.toml');
    const r = run(file);
    assert.equal(r.code, 1);
    const found = findingLines(r.out);
    assert.equal(found.length, 1, r.out);
    assert.ok(found[0].startsWith(`${file}(9): error: `), found[0]);
});

test('a folder checks every .toml in it, sorted by name, and ignores other files and subfolders', () => {
    const dir = fixture('folder');
    const r = run(dir);
    assert.equal(r.code, 0, r.out + r.err);
    const names = findingLines(r.out).map((l) => l.split('(')[0]);
    assert.deepEqual(names, [join(dir, 'Zeta.toml')]);  // the only file with a finding
    assert.match(r.out, /3 valid profile\(s\), 0 error\(s\), 1 warning\(s\)\./);
    assert.ok(!r.out.includes('notes.txt') && !r.out.includes('nested.toml'));
    const order = lines(r.out).filter((l) => l.includes('starting level')).map((l) => /^(.*): \S+\|0x/.exec(l)[1].slice(dir.length + 1));
    assert.deepEqual(order, ['alpha.TOML', 'good.toml', 'Zeta.toml']);
});

test('--json prints one array and nothing else', () => {
    const r = run('--json', fixture('unknown-key.toml'), fixture('clean.toml'), fixture('no-opinions.toml'));
    assert.equal(r.code, 1);
    assert.equal(r.err, '');
    const parsed = JSON.parse(r.out);
    assert.ok(Array.isArray(parsed));
    assert.deepEqual(parsed, [
        { file: fixture('unknown-key.toml'), line: 8, level: 'error', message: "unknown field 'likess'" },
        { file: fixture('unknown-key.toml'), line: 8, level: 'warning', message: "unknown key 'likess' (did you mean 'likes'?)" },
        { file: fixture('no-opinions.toml'), line: 7, level: 'warning', message: NO_OPINIONS },
    ]);
    assert.equal(JSON.parse(run('--json', fixture('clean.toml')).out).length, 0);
    assert.equal(run('--json', fixture('clean.toml')).out.trim(), '[]');
});

test('--json escapes backslashes in paths and quotes in messages', () => {
    const dir = join(temp, 'escape');
    mkdirSync(dir);
    const file = join(dir, 'quote.toml');
    writeFileSync(file, 'schema_version = 1\n[npc]\nplugin = "X.esp"\nform_id = "0x000900"\n"a\\"b" = 1\n[romance]\nlikes = ["Murders"]\n');
    const r = run('--json', file);
    assert.equal(r.code, 1);
    assert.ok(r.out.includes('\\\\'), 'backslashes in the path are escaped');
    assert.ok(r.out.includes('a\\"b'), 'the quote in the message is escaped');
    const parsed = JSON.parse(r.out);
    assert.equal(parsed[0].file, file);
    assert.ok(parsed.some((f) => f.message.includes('a"b')));
});

test('--strict: exit 2 only when there are warnings and no errors; errors win', () => {
    assert.equal(run('--strict', fixture('no-opinions.toml')).code, 2);
    assert.equal(run('--strict', fixture('no-opinions.toml'), fixture('broken.toml')).code, 1);
    assert.equal(run(fixture('no-opinions.toml'), fixture('unlock-long.toml')).code, 0);
});

test('a missing path is an error finding and the other paths are still checked', () => {
    const missing = join(temp, 'nope.toml');
    const r = run(missing, fixture('no-opinions.toml'));
    assert.equal(r.code, 1);
    const found = findingLines(r.out);
    assert.deepEqual(found, [`${missing}: error: no such file or folder`, `${fixture('no-opinions.toml')}(7): warning: ${NO_OPINIONS}`]);
    const j = JSON.parse(run('--json', missing).out);
    assert.deepEqual(j, [{ file: missing, line: 0, level: 'error', message: 'no such file or folder' }]);
});

test('an empty folder is not an error', () => {
    const dir = join(temp, 'empty');
    mkdirSync(dir);
    const r = run('--strict', dir);
    assert.equal(r.code, 0);
    assert.match(r.out, /0 valid profile\(s\), 0 error\(s\), 0 warning\(s\)\./);
});

test('a usage error prints the usage on stderr and exits 3', () => {
    for (const args of [[], ['--strict'], ['--frobnicate', fixture('clean.toml')], ['-x', fixture('clean.toml')]]) {
        const r = run(...args);
        assert.equal(r.code, 3, args.join(' '));
        assert.equal(r.out, '');
        assert.match(r.err, /^(Unknown option: .*\n)?Usage: romantasy-profile-check \[--strict\] \[--json\] \[--player\] \[--\] <file-or-folder>\.\.\./);
    }
    assert.match(run('--frobnicate', fixture('clean.toml')).err, /^Unknown option: --frobnicate\n/);
});

test('-- ends the options', () => {
    assert.equal(run('--', fixture('clean.toml')).code, 0);
    const r = run('--', '--strict');
    assert.equal(r.code, 1);
    assert.deepEqual(findingLines(r.out), ['--strict: error: no such file or folder']);
});

test('paths with spaces and non-ASCII characters work', () => {
    const dir = join(temp, 'sp ace éü 日本');
    mkdirSync(dir);
    copyFileSync(fixture('clean.toml'), join(dir, 'föllower one.toml'));
    copyFileSync(fixture('no-opinions.toml'), join(dir, 'two.toml'));
    const r = run(dir);
    assert.equal(r.code, 0, r.out + r.err);
    assert.ok(r.out.includes(`${join(dir, 'two.toml')}(7): warning: `), r.out);
    assert.match(r.out, /2 valid profile\(s\)/);
    const single = JSON.parse(run('--json', join(dir, 'two.toml')).out);
    assert.equal(single[0].file, join(dir, 'two.toml'));
});

test('two files for one NPC are both rejected, as when Skyrim loads them', () => {
    const dir = join(temp, 'dupes');
    mkdirSync(dir);
    copyFileSync(fixture('clean.toml'), join(dir, 'one.toml'));
    copyFileSync(fixture('clean.toml'), join(dir, 'two.toml'));
    const r = run(dir);
    assert.equal(r.code, 1);
    const found = findingLines(r.out);
    assert.equal(found.length, 2, r.out);
    assert.ok(found.every((l) => l.includes('error: conflicting profiles for fixture.esp|2049; all definitions rejected:')), r.out);
});

test('--player allows enabled and nothing else changes', () => {
    const dir = join(temp, 'player');
    mkdirSync(dir);
    const file = join(dir, 'p.toml');
    writeFileSync(file, 'schema_version = 1\n[npc]\nplugin = "X.esp"\nform_id = "0x000900"\n[romance]\nenabled = true\nlikes = ["Murders"]\n');
    assert.equal(run(file).code, 1);
    assert.equal(run('--player', file).code, 0);
    assert.equal(run(dir, '--player').code, 0);
});

test('a version 3 file with [moments] is clean under --strict', () => {
    const r = run('--strict', fixture('moments.toml'));
    assert.equal(r.code, 0, r.out + r.err);
    assert.deepEqual(findingLines(r.out), []);
    assert.match(r.out, /1 valid profile\(s\), 0 error\(s\), 0 warning\(s\)\./);
});

test('[moments] out of order: three warnings, exit 0, exit 2 under --strict', () => {
    const file = fixture('moments-out-of-order.toml');
    const r = run(file);
    assert.equal(r.code, 0, r.out);
    assert.deepEqual(findingLines(r.out), [
        `${file}(13): warning: moments: closeness is welcome before affection`,
        `${file}(14): warning: moments: intimacy is welcome before closeness`,
        `${file}(14): warning: moments: intimacy is welcome before affection`,
    ]);
    assert.equal(run('--strict', file).code, 2);
});

test('[moments] unknown key: the parser error and a suggestion', () => {
    const file = fixture('moments-unknown-key.toml');
    const r = run(file);
    assert.equal(r.code, 1);
    assert.deepEqual(findingLines(r.out), [
        `${file}(13): error: unknown field 'closenes'`,
        `${file}(13): warning: unknown key 'closenes' (did you mean 'closeness'?)`,
    ]);
});

test('[moments] unknown tier: an error naming the tier', () => {
    const file = fixture('moments-bad-tier.toml');
    const r = run(file);
    assert.equal(r.code, 1);
    assert.deepEqual(findingLines(r.out), [`${file}: error: unknown tier 'Freind' in [moments]`]);
});

test('[moments] in a version 2 file is an unknown field', () => {
    const file = fixture('moments-v2.toml');
    const r = run(file);
    assert.equal(r.code, 1);
    assert.deepEqual(findingLines(r.out), [
        `${file}(11): error: unknown field 'moments'`,
        `${file}(11): warning: unknown key 'moments'`,
    ]);
});

test('[moments] in a player profile is refused', () => {
    const file = fixture('moments-player.toml');
    assert.equal(run(file).code, 0);
    const r = run('--player', file);
    assert.equal(r.code, 1);
    assert.deepEqual(findingLines(r.out), [`${file}: error: [moments] is not allowed in a player profile`]);
});

test('every profile shipped in examples/profiles is clean under --strict', () => {
    const shipped = join(root, 'examples', 'profiles');
    const tomls = readdirSync(shipped).filter((n) => n.toLowerCase().endsWith('.toml'));
    assert.ok(tomls.length > 0, 'no shipped example profiles found');
    const r = run('--strict', shipped);
    assert.equal(r.code, 0, r.out + r.err);
    assert.deepEqual(findingLines(r.out), []);
    assert.match(r.out, new RegExp(`${tomls.length} valid profile\\(s\\)`));
});

let failed = 0;
for (const { name, fn } of tests) {
    try {
        fn();
        console.log(`ok   ${name}`);
    } catch (e) {
        failed++;
        console.log(`FAIL ${name}\n     ${String(e.message).split('\n').join('\n     ')}`);
    }
}
rmSync(temp, { recursive: true, force: true });
console.log(`${tests.length - failed} passed, ${failed} failed`);
process.exit(failed ? 1 : 0);
