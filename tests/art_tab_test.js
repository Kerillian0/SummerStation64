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
const t = new Function(html.slice(a, b) + 'return {parseMissingArt, fitInside, shortDesc, authorWith, mergeIni, artTarget, detailsPath, retroFolder, diskIdOf, detailsLacking, parseMetadat, crcCandidates, libretroDate, matchName, launchboxGames, mainName, launchboxMatch, faceFrame, fitLayout, otherEdges, cardTopOf};')();

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

console.log('Where the card\'s top is, from a plain folder upload (Firefox)');
let top = t.cardTopOf(['SD/menu/missingart.txt', 'SD/N64(US)/Mario.z64']);
check(top.prefix === 'SD/' && !top.menuOnly, 'the card picked: its name comes first');
top = t.cardTopOf(['E:/SD/menu/config.ini', 'E:/SD/N64/x.z64']);
check(top.prefix === 'E:/SD/', 'more than one folder before menu/');
top = t.cardTopOf(['menu/missingart.txt', 'N64/x.z64']);
check(top.prefix === '' && !top.menuOnly, 'nothing before menu/');
top = t.cardTopOf(['menu/missingart.txt', 'menu/metadata/N/S/M/E/boxart_front.png']);
check(top.prefix === '' && !top.menuOnly, 'the menu folder named first is the card\'s, not the picked one');
top = t.cardTopOf(['Backups/menu/config.ini', 'Backups/othermenu/config.ini']);
check(top.prefix === 'Backups/', 'a folder merely ending in "menu" is not the menu folder');
top = t.cardTopOf(['mymenu/missingart.txt', 'mymenu/metadata/a.png']);
check(top.prefix === 'mymenu/' && top.menuOnly, 'the menu folder picked: the report sits right inside it');
top = t.cardTopOf(['SD/N64/x.z64']);
check(top.prefix === 'SD/' && !top.menuOnly, 'no report anywhere: the first folder, as before');

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

console.log('Emulator games');
check(t.retroFolder('N64(US)/Super Mario Bros. (World).nes') === 'nes/Super Mario Bros. (World)', 'NES: its system, then the name without the folder or extension');
check(t.retroFolder('Zelda.SFC') === 'snes/Zelda' && t.retroFolder('Zelda.smc') === 'snes/Zelda', 'both SNES extensions, any case');
check(t.retroFolder('Tetris.gb') === 'gb/Tetris' && t.retroFolder('Pokemon.gbc') === 'gbc/Pokemon', 'Game Boy and Game Boy Color apart');
check(t.retroFolder('Sonic.sms') === null && t.retroFolder('Mario.z64') === null && t.retroFolder('.nes') === null, 'other systems, N64 games and odd names are not');

console.log('64DD disks and details');
(async () => {
  const block = 232 * 85, image = new Uint8Array(16 * block);
  image.set([0x44, 0x4D, 0x50, 0x4A], 15 * block);     // DMPJ in block 15 only
  const read = async (offset) => image.slice(offset, offset + 4);
  check(await t.diskIdOf(read) === 'DMPJ', 'the ID from block 15 when 14 is blank');
  image.set([0x45, 0x46, 0x5A, 0x4A], 14 * block);     // EFZJ in block 14 wins
  check(await t.diskIdOf(read) === 'EFZJ', 'block 14 first');
  check(await t.diskIdOf(async () => new Uint8Array([0, 1, 2, 3])) === null, 'not a disk: no ID');
  check(t.detailsLacking(null, false).join() === 'description,date,developer', 'no metadata.ini: all three');
  check(t.detailsLacking('[meta]\nauthor = Nintendo\nrelease-date = 1999-12-01\nshort-desc =\n', true).length === 0, 'a description.txt counts, as do the keys');
  check(t.detailsLacking('[meta]\nshort-desc = A game.\nauthor =\n', false).join() === 'date,developer', 'an empty key is missing');

  console.log('libretro');
  const dat = 'clrmamepro (\n\tname "x"\n)\n\ngame (\n\tcomment "10-Yard Fight (USA, Europe)"\n\tdeveloper "Irem"\n\trom ( crc C986CDA2 )\n)\n\n' +
    'game (\n\tcomment "10-Pin Bowling (USA)"\n\tusers 1\n\trom ( crc 9A024415 )\n)\n';
  let md = t.parseMetadat(dat, 'developer');
  check(md.byCrc.get('c986cda2') === 'Irem' && md.byName.get(t.matchName('10-Yard Fight (USA, Europe)')) === 'Irem', 'a developer, by CRC and by name');
  check(t.parseMetadat(dat, 'users').byCrc.get('9a024415') === '1', 'player counts without quotes');
  const crc = require('zlib').crc32 || ((b) => { let c = ~0; for (const x of b) { c ^= x; for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xEDB88320 & -(c & 1)); } return ~c >>> 0; });
  const nes = new Uint8Array(16 + 32); nes.set([0x4E, 0x45, 0x53, 0x1A]);
  let cands = t.crcCandidates(nes, crc);
  check(cands.length === 2 && cands[1] === ('00000000' + crc(nes.subarray(16)).toString(16)).slice(-8), 'an NES file also without its header');
  check(t.crcCandidates(new Uint8Array(1024 + 512), crc).length === 2, 'an SNES file with a copier header also without it');
  check(t.libretroDate('1985', '9') === '1985-09' && t.libretroDate('1994', '') === '1994' && t.libretroDate('', '3') === '', 'year and month as a date');

  console.log('LaunchBox');
  check(t.matchName('Legend of Zelda, The - Ocarina of Time (U) [!].z64') === t.matchName('The Legend of Zelda: Ocarina of Time'), 'names match without tags, articles or punctuation');
  check(t.matchName('Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (CGB+SGB Enhanced).gbc') === t.matchName('Pokémon Yellow Version: Special Pikachu Edition'), 'accents read as plain letters');
  check(t.matchName('Banjo & Kazooie.nes') === t.matchName('Banjo and Kazooie'), '& and "and"');
  const xml = '<LaunchBox>\n<Game>\n<Name>Super Mario Bros.</Name>\n<ReleaseDate>1985-09-13T00:00:00-07:00</ReleaseDate>\n<Overview>Mario &amp; Luigi.</Overview>\n' +
    '<MaxPlayers>2</MaxPlayers>\n<Platform>Nintendo Entertainment System</Platform>\n<Genres>Platform;Action</Genres>\n<Developer>Nintendo</Developer>\n</Game>\n' +
    '<Game>\n<Name>Halo</Name>\n<Platform>Microsoft Xbox</Platform>\n</Game>\n<Game>\n<Name>Tetr';
  const lb = t.launchboxGames(xml, ['Nintendo Entertainment System']);
  check(lb.games.length === 1 && lb.games[0].date === '1985-09-13' && lb.games[0].overview === 'Mario & Luigi.' && lb.games[0].genre === 'Platform', 'a game, its date, text and first genre');
  check(lb.rest === '<Game>\n<Name>Tetr', 'an unfinished game is kept for the next stretch');
  check(t.parseMetadat('game (\n\tcomment "Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (CGB+SGB Enhanced)"\n\tdeveloper "Game Freak"\n\trom ( crc 7D527D62 )\n)\n', 'developer')
        .byName.get(t.matchName('Pokemon Yellow Version - Special Pikachu Edition.gb')) === 'Game Freak', 'libretro by a tidy name when the CRC differs');
  const lbg = t.launchboxGames('<Game><Name>Sin and Punishment</Name><DatabaseID>7</DatabaseID><Platform>Nintendo 64</Platform></Game>' +
    '<GameAlternateName><AlternateName>Tsumi to Batsu: Hoshi no Keishousha</AlternateName><DatabaseID>7</DatabaseID></GameAlternateName><GameImage>x</Gam', ['Nintendo 64']);
  check(lbg.games.length === 1 && lbg.alternates.length === 1 && lbg.alternates[0].id === '7', 'alternate names read too');
  check(lbg.rest.length <= 32, 'a long tail of other blocks is not kept');
  const games = [{ names: ['Sin and Punishment', 'Tsumi to Batsu: Hoshi no Keishousha'] }, { names: ['Sin & Punishment: Star Successor'] }];
  check(t.launchboxMatch('Sin and Punishment - Hoshi no Keishousha (J) [!].z64', games) === games[0], 'the main title finds the one game');
  check(t.launchboxMatch('Tsumi to Batsu - Hoshi no Keishousha.z64', games) === games[0], 'an alternate name matches whole');
  check(t.launchboxMatch('Sin and Punishment.z64', [{ names: ['Sin and Punishment - A'] }, { names: ['Sin and Punishment - B'] }]) === null, 'two games fit the main title: no guess');

  console.log('Fit editor');
  let fr = t.faceFrame('front', 'game', 300, 200, 'auto');
  check(fr.w === 158 && fr.h === 112 && fr.fill, 'an N64 cover: the wide box frame, cropped to fill');
  check(t.faceFrame('front', 'game', 200, 300, 'auto').h === 158, 'a tall picture: the tall box frame');
  fr = t.faceFrame('top', 'game', 600, 100, 'auto');
  check(fr.w === 158 && fr.h === 30, 'an N64 top label as deep as the box (0.19 of its width)');
  check(t.faceFrame('spine', 'snes', 40, 300, 'auto').w === 40, 'an SNES spine: a quarter of the box height');
  check(!t.faceFrame('front', 'nes', 300, 420, 'auto').fill, 'a retro cover keeps its own shape');
  let L = t.fitLayout(400, 100, { w: 158, h: 30, fill: true }, 0, 1, 0, 0);
  check(Math.abs(L.h - 39.5) < 0.01 && Math.abs(L.cx - 79) < 0.01 && Math.abs(L.cy - 15) < 0.01, 'zoom 1 covers the frame, centred');
  L = t.fitLayout(400, 100, { w: 158, h: 30, fill: true }, 0, 1, 0, -1);
  check(Math.abs((L.cy + L.h / 2) - 30) < 0.01, 'up-down -1 moves the picture up: its bottom edge at the frame\'s bottom');
  L = t.fitLayout(100, 400, { w: 158, h: 30, fill: true }, 1, 1, 0, 0);
  check(L.odd && Math.abs(L.w - 158) < 0.01, 'a tall picture turned once lies along the strip');
  check(t.otherEdges('top', false).join() === 'boxart_left.png,boxart_right.png' && t.otherEdges('spine', true).join() === 'boxart_top.png' && !t.otherEdges('front', false).length, 'the other edges each kind of box takes');
  console.log(failures ? failures + ' of ' + checks + ' checks failed.' : checks + ' checks, 0 failed');
  process.exit(failures ? 1 : 0);
})();
