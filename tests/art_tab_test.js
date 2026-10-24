// PC-side tests for the Missing Art tab of docs/companion/index.html:
// reading missingart.txt, picture sizes, the short description, adding to
// an existing metadata.ini, and which folder is written. The code under
// test is taken straight out of the page. Run with tests/run.sh.
'use strict';
const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'docs', 'companion', 'index.html'), 'utf8');
const a = html.indexOf('// --- art logic'), b = html.indexOf('// --- end art logic ---');
if (a < 0 || b < 0) throw new Error('Could not find the logic block in the Missing Art tab; did it change?');
const t = new Function(html.slice(a, b) + 'return {parseMissingArt, fitInside, shortDesc, authorWith, mergeIni, artTarget, detailsPath};')();

let checks = 0, failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL: ' + what); }
}

console.log('missingart.txt');
// As art_report.c writes it.
const report = 'missingart 1\n# Games that lack art or details, made by SummerStation64. One game per line,\n' +
  '# tab-separated: art folder under menu/metadata, kind, what it lacks, the game\'s file.\n' +
  '# Lacks: front, back, top, cartridge, description, date, developer.\n' +
  'N/D/Z/J\tgame\tfront,back,top\tN64(JP)/Dezaemon 3D (J).z64\n' +
  'homebrew/the_big_fix\thomebrew\tfront,description,date,developer\tN64(US)/the_big_fix.z64\r\n' +
  'X/M/L/E\thack\tcartridge\tHacks/Zelda\twith a tab.z64\n';
const games = t.parseMissingArt(report);
check(games && games.length === 3, 'three games, comments left out');
check(games[0].folder === 'N/D/Z/J' && games[0].kind === 'game' && games[0].lacks.join() === 'front,back,top' &&
      games[0].file === 'N64(JP)/Dezaemon 3D (J).z64', 'its fields');
check(games[1].folder === 'homebrew/the_big_fix' && games[1].lacks.length === 4, 'homebrew by title, Windows line end');
check(games[2].file === 'Hacks/Zelda\twith a tab.z64', 'a file name holding a tab stays whole');
check(t.parseMissingArt('hacks 1\n') === null, 'another file is not taken for the report');

console.log('Pictures and text');
let f = t.fitInside(316, 44);
check(f.w === 158 && f.h === 22, 'a top label twice the pack\'s size comes out 158 x 22');
f = t.fitInside(300, 600);
check(f.w === 79 && f.h === 158, 'a tall cartridge fits the height');
f = t.fitInside(50, 50);
check(f.w === 158 && f.h === 158, 'a small picture is scaled up to fit');
check(t.shortDesc('A short one.') === 'A short one.', 'a short description stays');
const long = 'In a world of giant robots, '.repeat(10);
const cut = t.shortDesc(long);
check(cut.length <= 120 && /\.\.\.$/.test(cut) && !/ \.\.\.$/.test(cut), 'a long one is cut at a word, with "..."');
check(t.authorWith('Athena | Athena', 'Athena Co.') === 'Athena Co. | Athena', 'the publisher stays');
check(t.authorWith('', 'Eurocom') === 'Eurocom', 'no publisher');

console.log('metadata.ini');
const pack = '[meta]\r\nname = Dezaemon 3D\r\nauthor = | Athena\r\nshort-desc = \r\n\r\n[custom]\r\nkey = 1\r\n';
const merged = t.mergeIni(pack, { 'release-date': '1998-07-24', author: 'Athena | Athena', 'short-desc': '' });
check(/^\[meta\]\nname = Dezaemon 3D\nauthor = Athena \| Athena\nshort-desc = \nrelease-date = 1998-07-24\n\n\[custom\]\nkey = 1\n$/.test(merged),
      'keys set in place or added in [meta], other sections kept, empty values not written');
check(t.mergeIni(null, { name: 'The Big Fix', 'short-desc': 'A fix.' }) === '[meta]\nname = The Big Fix\nshort-desc = A fix.\n', 'a new file');
check(t.mergeIni('name = Old\n', { author: 'Me' }) === '[meta]\nname = Old\nauthor = Me\n', 'a file without [meta] gets one');

console.log('Which folder');
let w = t.artTarget('N/D/Z/J', false, true, true);
check(w.write === 'N/D/Z/J' && w.copyFrom === null, 'Dezaemon 3D: the folder without the letter is other games\' (Duke Nukem), nothing copied');
w = t.artTarget('N/A/B/E', false, true, false);
check(w.copyFrom === 'N/A/B', 'a game using the folder without the letter: its files come along');
w = t.artTarget('N/A/B/E', true, true, false);
check(w.copyFrom === null, 'its own folder there already: nothing copied');
w = t.artTarget('homebrew/the_big_fix', false, false, false);
check(w.write === 'homebrew/the_big_fix' && w.copyFrom === null, 'homebrew');

console.log('Where details go');
check(t.detailsPath(games[0]) === 'menu/metadata/N/D/Z/J/metadata.ini', 'a game: its folder');
check(t.detailsPath(games[1]) === 'N64(US)/the_big_fix.metadata.ini', 'homebrew: beside its file, as Game info reads it');
check(t.shortDesc(long, 250).length <= 250 && t.shortDesc(long, 250).length > 120, 'homebrew\'s longer short description');

console.log(checks + ' checks, ' + failures + ' failed');
process.exit(failures ? 1 : 0);
