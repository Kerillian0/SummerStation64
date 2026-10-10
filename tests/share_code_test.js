// PC-side tests for the Theme Maker's share codes (theme-maker/theme-maker.html).
// The code under test is taken straight out of the page, so these check the
// real thing, not a copy. Run with tests/run.sh (needs Node.js).
'use strict';
const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'theme-maker', 'theme-maker.html'), 'utf8');
function between(from, to) {
  const a = html.indexOf(from), b = html.indexOf(to, a);
  if (a < 0 || b < 0) throw new Error('Could not find "' + from + '" in the Theme Maker; did it change?');
  return html.slice(a, b);
}
const source =
  between('var FRAME_STOCK', 'var PRESETS') +
  'var S = Object.assign({}, DEFAULTS);\n' +
  between('var B32', 'function updateShareCode') +
  'return {encodeTheme: encodeTheme, decodeTheme: decodeTheme, crc16: crc16, S: S, DEFAULTS: DEFAULTS, FEATURES: FEATURES, FRAME_KEYS: FRAME_KEYS};';
const tm = new Function(source)();

let checks = 0, failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL: ' + what); }
}
function reset(changes) {
  Object.keys(tm.S).forEach(k => delete tm.S[k]);
  Object.assign(tm.S, tm.DEFAULTS, changes || {});
}
function roundTrip(name, changes, version) {
  console.log(name);
  reset(changes);
  const code = tm.encodeTheme();
  const r = tm.decodeTheme(code);
  check(!r.error, 'decodes without an error (' + r.error + ')');
  if (r.error) return code;
  check(r.version === version, 'is a v' + version + ' code (got v' + r.version + ')');
  const keys = ['text', 'textDim', 'accent', 'panel', 'c1', 'c2', 'c3', 'pColor', 'bgType', 'dir', 'useC3', 'dither', 'pattern', 'pSize', 'pOpacity']
    .concat(tm.FRAME_KEYS, tm.FEATURES.map(f => 'f_' + f.key));
  keys.forEach(k => check(String(r.theme[k]).toUpperCase() === String(tm.S[k]).toUpperCase(), k + ' comes back the same (' + tm.S[k] + ' -> ' + r.theme[k] + ')'));
  return code;
}

console.log('CRC-16/CCITT-FALSE matches its published check value');
check(tm.crc16([...'123456789'].map(c => c.charCodeAt(0)), 9) === 0x29B1, 'crc16("123456789") is 0x29B1');

const v1 = roundTrip('The default theme makes a short (v1) code that loads back the same', {}, 1);
check(/^SS64-/.test(v1), 'starts with SS64-');
check(v1.replace(/[^0-9A-Z]/g, '').length === 4 + 52, 'is 52 characters after the prefix');

roundTrip('Every color and setting changed, stock frame: still v1', {
  text: '#102030', textDim: '#405060', accent: '#708090', panel: '#A0B0C0',
  c1: '#D0E0F0', c2: '#010203', c3: '#FEDCBA', useC3: true, pColor: '#00FF00',
  bgType: 'solid', dir: 'radial', dither: false, pattern: 'checker', pSize: 32, pOpacity: 40
}, 1);

roundTrip('A frame color changed: v2', {border: '#123456'}, 2);
roundTrip('A first-eight feature set: v2', {f_quick_launch: 1, f_frame_borders: 0, f_see_through_covers: 1}, 2);
const v3 = roundTrip('A game-name feature set: v3', {f_hide_tags: 1, f_tidy_titles: 0, f_side_covers: 1}, 3);
check(v3.replace(/[^0-9A-Z]/g, '').length === 4 + 88, 'v3 is 88 characters after the prefix');

console.log('Typing slips are forgiven where the alphabet allows');
reset({accent: '#ABCDEF'});
const code = tm.encodeTheme();
check(!tm.decodeTheme(code.toLowerCase()).error, 'lower case');
check(!tm.decodeTheme(code.replace(/-/g, ' ')).error, 'spaces instead of dashes');
check(!tm.decodeTheme(code.slice(5)).error, 'without the SS64- prefix');
const withO = code.replace(/0/g, 'O');
check(withO === code || !tm.decodeTheme(withO).error, 'the letter O for zero');

console.log('Damaged codes are refused with a reason');
const body = code.slice(5).replace(/-/g, '');
const swap = c => (c === 'A' ? 'B' : 'A');
const typo = body.slice(0, 10) + swap(body[10]) + body.slice(11);
check(/typo/.test(tm.decodeTheme(typo).error || ''), 'one wrong character is caught by the check sum');
check(/wrong length/.test(tm.decodeTheme(body.slice(0, -1)).error || ''), 'a missing character');
check(/wrong length/.test(tm.decodeTheme('').error || ''), 'an empty code');
check(/allowed/.test(tm.decodeTheme(body.slice(0, -1) + 'U').error || ''), 'a letter Crockford base32 leaves out (U)');

console.log('Every single-character typo in a code is caught');
// The last character also carries a few unused filler bits; a slip there
// that leaves the theme the same is harmless, so only a code that loads as a
// *different* theme counts as missed.
const original = JSON.stringify(tm.decodeTheme(code).theme);
let missed = 0;
for (let i = 0; i < body.length; i++) {
  const t = body.slice(0, i) + (body[i] === 'Z' ? 'Y' : 'Z') + body.slice(i + 1);
  const r = tm.decodeTheme(t);
  if (!r.error && JSON.stringify(r.theme) !== original) missed++;
}
check(missed === 0, missed + ' of ' + body.length + ' single-character typos slipped through');

console.log('share codes: ' + checks + ' checks, ' + failures + ' failed');
process.exit(failures ? 1 : 0);
