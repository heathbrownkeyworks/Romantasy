import { readdirSync } from 'node:fs';

const here = new URL('./', import.meta.url);
for (const file of readdirSync(here).filter(f => f.endsWith('.mjs') && f !== 'all.mjs').sort()) {
    await import(new URL(file, here));
}
console.log('view tests: ok');
