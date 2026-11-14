// The pad's pure rules: the back chain, the close chord, typing and the legend.
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const s = require('../../view/js/state.js');
const p = require('../../view/js/pad.js');

const v = s.createView();
assert.equal(p.layer(v), 'ledger'); assert.equal(p.back(v), 'close');
v.pane = 'settings'; assert.equal(p.layer(v), 'settings'); assert.equal(p.back(v), 'close-settings');
v.takeover = { mode: 'create' }; assert.equal(p.layer(v), 'takeover'); assert.equal(p.back(v), 'close-takeover');
v.modal = { kind: 'reset' }; assert.equal(p.layer(v), 'modal'); assert.equal(p.back(v), 'close-modal');
v.popup = { followerName: 'A' }; assert.equal(p.layer(v), 'popup'); assert.equal(p.back(v), 'dismiss-popup');

const chord = { code: 'KeyR', ctrlKey: true, altKey: false, shiftKey: false, metaKey: false };
assert.equal(p.isCloseChord(chord, { openWithFavorites: false }, false), true);
assert.equal(p.isCloseChord(chord, { openWithFavorites: true }, false), false);
assert.equal(p.isCloseChord(chord, { openWithFavorites: false }, true), false);
assert.equal(p.isCloseChord({ ...chord, shiftKey: true }, { openWithFavorites: false }, false), false);
assert.equal(p.isCloseChord({ ...chord, altKey: true }, { openWithFavorites: false }, false), false);
assert.equal(p.isCloseChord({ ...chord, metaKey: true }, { openWithFavorites: false }, false), false);
assert.equal(p.isCloseChord({ ...chord, ctrlKey: false }, { openWithFavorites: false }, false), false);
assert.equal(p.isCloseChord({ ...chord, code: 'KeyT' }, { openWithFavorites: false }, false), false);

assert.equal(p.isTyping({ tagName: 'INPUT', disabled: false }), true);
assert.equal(p.isTyping({ tagName: 'TEXTAREA', disabled: false }), true);
assert.equal(p.isTyping({ tagName: 'INPUT', disabled: true }), false);
assert.equal(p.isTyping({ tagName: 'BUTTON', disabled: false }), false);
assert.equal(p.isTyping(null), false);

const labels = (view, device) => p.legend(view, device).map(x => x.label);
const prompts = (view, device) => p.legend(view, device).map(x => x.prompt);
assert.deepEqual(labels(s.createView(), 'keyboard'), ['Select', 'Open', 'Close']);
assert.deepEqual(prompts(s.createView(), 'keyboard'), ['↑↓', 'Enter', 'Esc']);
assert.deepEqual(labels(v, 'keyboard'), ['Continue']);
assert.deepEqual(prompts(v, 'keyboard'), ['Enter']);
const at = patch => Object.assign(s.createView(), patch);
assert.deepEqual(p.legend(at({ pane: 'settings' }), 'keyboard'), [{ prompt: 'Enter', label: 'Select' }, { prompt: 'Esc', label: 'Back' }]);
assert.deepEqual(p.legend(at({ takeover: { mode: 'edit' } }), 'keyboard'), [{ prompt: 'Enter', label: 'Stamp' }, { prompt: 'Esc', label: 'Cancel' }]);
assert.deepEqual(p.legend(at({ modal: { kind: 'password' } }), 'keyboard'), [{ prompt: 'Enter', label: 'Confirm' }, { prompt: 'Esc', label: 'Cancel' }]);
// Without the helper (Node), the gamepad prompts fall back to the default bindings' Xbox labels.
assert.deepEqual(p.legend(s.createView(), 'gamepad'), [{ prompt: 'D-pad ↑↓', label: 'Select' }, { prompt: 'A', label: 'Open' }, { prompt: 'B', label: 'Close' }]);
assert.deepEqual(p.legend(v, 'gamepad'), [{ prompt: 'A', label: 'Continue' }]);

console.log('Input: ok');
