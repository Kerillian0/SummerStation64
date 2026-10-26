// PC-side tests for the Cheats tab of docs/companion/index.html: reading
// libretro's .cht lists and .datel files, writing .datel the way
// src/menu/datel_codes.c reads it, and finding a game's list. The code under
// test is taken straight out of the page. Run with tests/run.sh.
'use strict';
const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'docs', 'companion', 'index.html'), 'utf8');
const a = html.indexOf('// --- cheats logic'), b = html.indexOf('// --- end cheats logic ---');
if (a < 0 || b < 0) throw new Error('Could not find the logic block in the Cheats tab; did it change?');
const t = new Function(html.slice(a, b) + 'return {parseCode, parseCht, parseDatel, formatDatel, linesUsed, cheatNote, mergeCheats, plainName, chtCandidates, pageOf};')();

let checks = 0, failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL: ' + what); }
}

console.log('libretro .cht');
const cht = 'cheats = 3\n\ncheat0_desc = "Infinite Lives"\ncheat0_code = "8033B21D 0064"\ncheat0_enable = false\n\n' +
  'cheat1_desc = "Have 120 Stars"\ncheat1_code = "80207723 0001+8020770B 00C7"\ncheat1_enable = false\n\n' +
  'cheat2_desc = "Broken"\ncheat2_code = "ZZZZ"\ncheat2_enable = false\n';
const list = t.parseCht(cht);
check(list.length === 2, 'two usable cheats, the broken one left out');
check(list[1].codes.length === 2 && list[1].codes[1].addr === '8020770B' && list[1].codes[1].val === '00C7', 'a "+" list becomes one cheat of two codes');
check(list.every(function (c) { return !c.on; }), 'nothing ticked at first');

console.log('.datel');
list[0].on = true; list[1].on = true;
const datel = t.formatDatel(list);
const lines = datel.trim().split('\n');
check(lines[0][0] === '#', 'a comment first (the menu skips # lines)');
check(lines[1] === '8033B21D 0064 Infinite Lives' && lines[2] === '80207723 0001 Have 120 Stars' && lines[3] === '8020770B 00C7', 'the name on each cheat\'s first line only');
const back = t.parseDatel(datel);
check(back.length === 2 && back[1].codes.length === 2 && back[1].desc === 'Have 120 Stars' && back[0].on, 'read back the same');
const off = t.parseDatel(': 8033B21D 0064 Lives\n8033B21E 0001\n');
check(off.length === 1 && !off[0].on && off[0].codes.length === 2, '": " marks a cheat switched off; a nameless line joins it');
check(t.linesUsed(list) === 3, 'three code lines ticked');
check(t.parseDatel('8033b21d:0064 lower case\n// comment\n; another\n').length === 1, 'lower case and a colon are read; comments skipped');

console.log('Notes and merging');
check(t.cheatNote({ desc: 'Enable Code (Must Be On)', codes: [{ addr: 'F1096820', val: '2400' }] }) === 'enable', 'an enable code');
check(t.cheatNote({ desc: 'Moon Jump', codes: [{ addr: 'D0071337', val: '0020' }, { addr: '8010DAF4', val: '0043' }] }) === 'button', 'a button code');
check(t.cheatNote({ desc: 'X', codes: [{ addr: 'DE000400', val: '0000' }] }) === 'skipped', 'DE codes are skipped by the menu');
const merged = t.mergeCheats([{ desc: 'Lives', codes: [{ addr: '8033B21D', val: '0064' }], on: true }], t.parseCht(cht));
check(merged.length === 2 && merged[0].on, 'a list fetched again adds only new cheats and keeps ticks');

console.log('Finding a game\'s list');
const names = ['Super Mario 64 (USA).cht', 'Super Mario 64 (Europe) (En,Fr,De).cht', 'Super Mario 64 (Japan).cht', 'SUPER MARIO 64.cht', 'Mario Kart 64 (USA).cht'];
let c = t.chtCandidates(names, 'Super Mario 64 (USA)', 'Super Mario 64 (U) [!].z64');
check(c[0] === 'Super Mario 64 (USA).cht', 'the exact No-Intro name first');
c = t.chtCandidates(names, '', 'Super Mario 64 (E) [!].z64');
check(c[0] === 'Super Mario 64 (Europe) (En,Fr,De).cht' && c.length === 4, 'no Card Prep: by plain name, the game\'s region first');
c = t.chtCandidates(names.concat(['F-Zero X (USA).cht']), '', '0222 - F-Zero X.z64');
check(c[0] === 'F-Zero X (USA).cht', 'a numbered file name ("0222 - ") still finds its list');
c = t.chtCandidates(names, '', 'Space Station Silicon Valley (U).z64');
check(c.length === 0, 'nothing for a game without a list');
check(t.plainName('Legend of Zelda, The - Ocarina of Time (U) (V1.2) [!].z64') === t.plainName('The Legend of Zelda - Ocarina of Time (USA).cht'), 'a trailing article and tags are left out');

console.log('Pages');
const many = Array.from({ length: 123 }, function (_, i) { return i; });
let pg = t.pageOf(many, 2, 50);
check(pg.pages === 3 && pg.items.length === 23 && pg.items[0] === 100, 'the last page holds what is left');
pg = t.pageOf(many, 9, 50);
check(pg.page === 2, 'a page past the end shows the last one');
pg = t.pageOf(many.filter(function (i) { return i % 40 === 0; }), 1, 50);
check(pg.page === 0 && pg.items.length === 4 && pg.items[3] === 120, 'search results from every page, on one page');
pg = t.pageOf([], 0, 50);
check(pg.pages === 1 && pg.items.length === 0, 'an empty list is one empty page');

console.log(failures ? failures + ' of ' + checks + ' checks failed.' : checks + ' checks, 0 failed');
process.exit(failures ? 1 : 0);
