// PC-side tests for the Collections tab of docs/companion/index.html:
// reading and writing collections.txt the way my_collections.c does, names
// cut to the menu's length, and which folders the Games tab reads. The code
// under test is taken straight out of the page. Run with tests/run.sh.
'use strict';
const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'docs', 'companion', 'index.html'), 'utf8');
const a = html.indexOf('// --- collections logic'), b = html.indexOf('// --- end collections logic ---');
if (a < 0 || b < 0) throw new Error('Could not find the logic block in the Collections tab; did it change?');
const t = new Function(html.slice(a, b) +
  'return {parseCollections, writeCollections, cutName, iniGet, tidyFolder, gameFolders, entryName, isGameFile, parseHidden, moveCollection};')();

let checks = 0, failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL: ' + what); }
}

console.log('collections.txt');
// As my_collections.c writes it, with a Windows line end and a comment added by hand.
const file = 'collections 1\n= Couch Coop\nN64(US)/Mario Kart 64 (U) [!].z64\r\nN64(US)/Mario Party 2 (U) [!].z64\n' +
  '# a note\n\n= Retro Night\nGoldenEye 007 (U) [!].z64\n';
let list = t.parseCollections(file);
check(list && list.length === 2, 'two collections');
check(list[0].name === 'Couch Coop' && list[0].games.length === 2 && list[0].games[0] === 'N64(US)/Mario Kart 64 (U) [!].z64', 'names and games, line ends dropped');
check(list[1].games.join() === 'GoldenEye 007 (U) [!].z64', 'blank and # lines skipped; a game in the top folder');
check(t.writeCollections(list) === file.replace('\r', '').replace('# a note\n\n', ''), 'written back as the menu writes it');
check(t.parseCollections('hidden 1\n') === null, 'another file is not taken for it');
check(t.parseCollections('collections 1\nGame.z64\n= A\n')[0].games.length === 0, 'games before the first name are left out, as on the console');
let big = 'collections 1\n';
for (let i = 0; i < 40; i++) big += '= C' + i + '\n';
big += Array.from({ length: 200 }, (_, i) => 'G' + i + '.z64').join('\n') + '\nG0.z64\n';
list = t.parseCollections(big);
check(list.length === 32, 'at most 32 collections');
check(list[31].name === 'C31' && list[31].games.length === 0, 'games under a dropped collection are not given to the last kept one');
big = 'collections 1\n= A\n' + Array.from({ length: 200 }, (_, i) => 'G' + i + '.z64').join('\n') + '\n';
check(t.parseCollections(big)[0].games.length === 128, 'at most 128 games');
check(t.parseCollections('collections 1\n= A\nG.z64\nG.z64\n')[0].games.length === 1, 'a game listed twice counts once');

console.log('Names');
check(t.cutName('  Couch   Coop\n') === 'Couch Coop', 'one line, spaces tidied');
check(t.cutName('A'.repeat(40)) === 'A'.repeat(28), '28 letters at most');
const accents = t.cutName('é'.repeat(20));
check(new TextEncoder().encode(accents).length <= 28 && accents === 'é'.repeat(14), 'cut between characters, never inside one');
check(t.cutName('Mario Party Night' + ' '.repeat(12) + 'X') === 'Mario Party Night X', 'spaces counted after tidying');

console.log('Games folders');
const config = '[menu]\nschema_revision = 3\ndefault_directory = /N64(US)/\n\n[autoload]\nrom_path = /x.z64\n';
check(t.iniGet(config, 'menu', 'default_directory') === '/N64(US)/', 'the start folder from config.ini');
check(t.iniGet(config, 'autoload', 'default_directory') === null, 'only from its section');
let folders = t.gameFolders(config, '/N64(JP)\n/n64(us)\nnot a folder\n/64DD/\n');
check(folders.join('|') === '/N64(US)|/N64(JP)|/64DD', 'start folder first, each once (case left aside), tidied');
check(t.gameFolders(null, null).join() === '/', 'no config.ini: the top of the card');
check(t.tidyFolder('sd:/Games/N64/') === '/Games/N64', 'the sd: prefix and the end slash dropped');
check(t.entryName('/', 'Game.z64') === 'Game.z64' && t.entryName('/N64(JP)', 'Game.z64') === 'N64(JP)/Game.z64', 'names from the top of the card');
check(t.isGameFile('Game.Z64') && t.isGameFile('Disk.ndd') && t.isGameFile('Zelda.gbc'), 'ROMs, disks and emulator games');
check(!t.isGameFile('Game.sav') && !t.isGameFile('readme.txt') && !t.isGameFile('._Game.z64'), 'saves, text and Mac leftovers are not games');

console.log('Hidden games and order');
check(t.parseHidden('hidden 1\nN64(US)/A.z64\n').has('N64(US)/A.z64'), 'hidden.txt read');
check(t.parseHidden('nonsense\nA.z64').size === 0, 'another file is not');
list = [{ name: 'Old' }, { name: 'Mid' }, { name: 'New' }];
check(t.moveCollection(list, 1, true) && list.map(c => c.name).join() === 'Old,New,Mid', 'up moves later in the file (the console shows the last first)');
check(!t.moveCollection(list, 2, true) && !t.moveCollection(list, 0, false), 'nothing past either end');

console.log(failures ? failures + ' of ' + checks + ' checks failed.' : 'All ' + checks + ' checks passed.');
process.exit(failures ? 1 : 0);
