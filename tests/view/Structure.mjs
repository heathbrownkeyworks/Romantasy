import assert from 'node:assert/strict';
import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { dirname, extname, join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const VIEW = resolve(dirname(fileURLToPath(import.meta.url)), '../../view');
const read = file => readFileSync(file, 'utf8');
const list = dir => existsSync(dir) ? readdirSync(dir).map(f => join(dir, f)) : [];
function walk(dir) {
    return list(dir).flatMap(f => statSync(f).isDirectory() ? walk(f) : [f]);
}
const rel = f => relative(VIEW, f).split('\\').join('/');

const indexPath = join(VIEW, 'index.html');
assert.ok(existsSync(indexPath), 'view/index.html missing');
const html = read(indexPath);

// The ledger loads every stylesheet and script it ships, and everything it names exists. The whisper
// page (whisper.html, a HUD card in its own view) loads its own two files; the ledger never loads them.
const refs = [...html.matchAll(/\s(?:src|href)="([^"]+)"/g)].map(m => m[1]);
const whisperPath = join(VIEW, 'whisper.html');
assert.ok(existsSync(whisperPath), 'view/whisper.html missing');
const whisperHtml = read(whisperPath);
const whisperRefs = [...whisperHtml.matchAll(/\s(?:src|href)="([^"]+)"/g)].map(m => m[1]);
const WHISPER_OWN = ['css/whisper.css', 'js/whisper.js'];
for (const own of WHISPER_OWN) {
    assert.ok(whisperRefs.includes(own), `whisper.html does not load ${own}`);
    assert.ok(!refs.includes(own), `index.html loads the whisper page's ${own}`);
}
for (const file of list(join(VIEW, 'css')).concat(list(join(VIEW, 'js')))) {
    if (WHISPER_OWN.includes(rel(file))) continue;
    assert.ok(refs.includes(rel(file)), `index.html does not load ${rel(file)}`);
}
for (const ref of refs) assert.ok(existsSync(join(VIEW, ref)), `index.html names ${ref}, which does not exist`);
for (const ref of whisperRefs) assert.ok(existsSync(join(VIEW, ref)), `whisper.html names ${ref}, which does not exist`);
const scripts = [...html.matchAll(/<script\s+src="([^"]+)"/g)].map(m => m[1]);
assert.equal(scripts[0], 'js/bridge.js', 'bridge.js loads first');
assert.equal(scripts[scripts.length - 1], 'js/app.js', 'app.js loads last');

// The ids later tasks render into, each exactly once; the page starts asleep.
for (const id of ['app', 'sky', 'petals', 'roster', 'pane', 'takeover', 'modal', 'popup', 'notice', 'legend']) {
    const count = (html.match(new RegExp(`\\sid="${id}"`, 'g')) || []).length;
    assert.equal(count, 1, `#${id} occurs ${count} times`);
}
assert.match(html, /<html[^>]*\sclass="asleep"/, '<html> starts with class="asleep"');

// Ultralight formats only: WOFF fonts, no WOFF2 or WebP; every CSS url() resolves.
const shipped = walk(VIEW).filter(f => !rel(f).startsWith('dev/'));
for (const file of shipped) assert.ok(!['.woff2', '.webp'].includes(extname(file)), `${rel(file)} is a format Ultralight cannot draw`);
assert.ok(list(join(VIEW, 'fonts')).some(f => extname(f) === '.woff'), 'no .woff font bundled');
const css = list(join(VIEW, 'css')).filter(f => f.endsWith('.css'));
for (const file of css) {
    for (const m of read(file).matchAll(/url\(\s*['"]?([^'")]+)['"]?\s*\)/g)) {
        assert.ok(existsSync(join(dirname(file), m[1])), `${rel(file)}: url(${m[1]}) does not resolve`);
    }
}

// What the renderer cannot draw, and what the page must never do.
const FORBIDDEN = [/backdrop-filter/, /box-shadow[^;]*inset/, /clip-path\s*:\s*polygon/, /toLocaleString|Intl\./,
    /\bfetch\s*\(|XMLHttpRequest/, /https?:\/\//, /<select|type=["']range["']|<audio|<video/];
const code = css.concat(list(join(VIEW, 'js')).filter(f => f.endsWith('.js')), [indexPath, whisperPath]);
// One file per page names the host: bridge.js for the ledger, whisper.js for the whisper card.
const HOST_LINKS = ['js/bridge.js', 'js/whisper.js'];
for (const file of code) {
    const text = read(file);
    for (const pattern of FORBIDDEN) assert.ok(!pattern.test(text), `${rel(file)} matches ${pattern}`);
    if (!HOST_LINKS.includes(rel(file))) assert.ok(!/window\.magelight/.test(text), `${rel(file)} names window.magelight`);
}

// Motion is transform and opacity only: every property every @keyframes block animates.
for (const file of css) {
    const text = read(file).replace(/\/\*[\s\S]*?\*\//g, '');
    for (const m of text.matchAll(/@keyframes\s+([\w-]+)\s*\{/g)) {
        let depth = 1, i = m.index + m[0].length;
        while (depth > 0 && i < text.length) { if (text[i] === '{') depth++; else if (text[i] === '}') depth--; i++; }
        const body = text.slice(m.index + m[0].length, i - 1);
        for (const frame of body.matchAll(/\{([^{}]*)\}/g)) {
            for (const declaration of frame[1].split(';')) {
                const property = declaration.split(':')[0].trim();
                if (property === '') continue;
                assert.ok(['transform', 'opacity'].includes(property), `${rel(file)}: @keyframes ${m[1]} animates ${property}`);
            }
        }
    }
}

// Every icon the page asks for.
for (const key of ['heart', 'heartbroken', 'stranger', 'acquaintance', 'friend', 'confidant', 'lover', 'spouse', 'likes', 'dislikes', 'newbond', 'settings', 'rays',
    'cat-battle', 'cat-blood', 'cat-crime', 'cat-exploration', 'cat-influence', 'cat-magic', 'cat-quests',
    'refresh', 'search', 'eye', 'star', 'lock', 'neutral', 'empty-ledger']) {
    assert.ok(existsSync(join(VIEW, 'icons', key + '.png')), `icons/${key}.png missing`);
}

console.log('Structure: ok');
