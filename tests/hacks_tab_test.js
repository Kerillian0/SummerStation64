// PC-side tests for the Hacks tab of docs/companion/index.html: hacks.txt,
// metadata.ini, the code suggestion, cover sizes and the .zip. The code under
// test is taken straight out of the page. Run with tests/run.sh.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const html = fs.readFileSync(path.join(__dirname, '..', 'docs', 'companion', 'index.html'), 'utf8');
const a = html.indexOf('// --- hacks logic'), b = html.indexOf('// --- end hacks logic ---');
if (a < 0 || b < 0) throw new Error('Could not find the logic block in the Hacks tab; did it change?');
const h = new Function(html.slice(a, b) +
  'return {parseHacks, formatHacks, nameTooLong, tidyName, codeOk, codeFolder, suggestCode, iniValue, metadataIni, parseMetaIni, coverCut, zipStore};')();

let checks = 0, failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL: ' + what); }
}

console.log('hacks.txt');
const kit = 'hacks 1\n; size crc32 code file name   (a CRC of 0: match by file name and size)\n' +
  '16777216 0 XMLE Legend of Zelda, The Missing Link (U) (V1.3).z64\n' +
  '33554432 0 XSPE Legend of Zelda, The Sealed Palace (U) (V1.3).z64\n';
const parsed = h.parseHacks(kit);
check(parsed && parsed.length === 2, 'the test kit\'s file is read');
check(parsed[0].code === 'XMLE' && parsed[0].size === 16777216 && parsed[0].crc === 0 &&
      parsed[0].name === 'Legend of Zelda, The Missing Link (U) (V1.3).z64', 'its fields');
check(h.formatHacks(parsed) === kit, 'written back the same');
check(h.parseHacks('hacks 1\r\n16777216 0c1d2e3f xmle Game.z64\r\n')[0].crc === 0x0c1d2e3f, 'Windows line ends, a CRC, a small-letter code');
check(h.parseHacks('hacks 1\n16777216 0c1d2e3f XMLE Game.z64\n')[0].code === 'XMLE', 'code kept');
check(h.formatHacks([{ size: 8, crc: 0x00ab, code: 'XABE', name: 'A b.z64' }]).split('\n')[2] === '8 000000ab XABE A b.z64',
      'a CRC is written as eight digits, as the menu reads it (%lx)');
check(h.parseHacks('verified 1\n') === null, 'another file is not taken for hacks.txt');
check(h.nameTooLong('x'.repeat(95)) === false && h.nameTooLong('x'.repeat(96)) === true, 'the menu\'s name limit (95)');

console.log('Names and codes');
check(h.tidyName('Legend of Zelda, The - Missing Link (U) (V1.3) [h1].z64') === 'The Legend of Zelda - Missing Link', 'a file name tidied');
check(h.tidyName('Super_Mario_64_Star_Road.z64') === 'Super Mario 64 Star Road', 'underscores');
check(h.suggestCode('The Legend of Zelda: The Missing Link', new Set()) === 'XMLE', 'Missing Link -> XMLE');
check(h.suggestCode('The Legend of Zelda: The Sealed Palace', new Set()) === 'XSPE', 'Sealed Palace -> XSPE');
check(h.suggestCode('Super Mario 64 - Star Road', new Set()) === 'XSRE', 'after a dash');
check(h.suggestCode('Star Road', new Set(['XSRE'])) === 'XSTE', 'a taken code is skipped');
check(h.suggestCode('', new Set()) === 'XAAE', 'no name still gives a code');
check(h.codeOk('XMLE') && !h.codeOk('XML') && !h.codeOk('xmle') && !h.codeOk('XM-E'), 'codes are four capitals or digits');
check(h.codeFolder('XMLE') === 'menu/metadata/X/M/L/E', 'the metadata folder');

console.log('metadata.ini');
const ini = h.metadataIni({ name: 'The Legend of Zelda: The Missing Link', author: 'Team; #1', date: '2023-04-01',
                            players: '1', short: 'Link\'s new\nquest' });
check(ini === '[meta]\nname = The Legend of Zelda: The Missing Link\nauthor = Team, 1\nrelease-date = 2023-04-01\n' +
      'num-players = 1\nshort-desc = Link\'s new quest\n', 'written as the menu reads it');
check(h.metadataIni({ name: '"Quoted"', date: '2023', players: '' }) === '[meta]\nname = Quoted"\n', 'no leading quote, unfinished date left out');
const back = h.parseMetaIni(ini);
check(back.name === 'The Legend of Zelda: The Missing Link' && back.date === '2023-04-01' && back.players === '1', 'read back');

console.log('Cover sizes');
let c = h.coverCut(316, 224, 'auto');
check(c.w === 158 && c.h === 112 && Math.abs(c.sw - 316) < 0.5 && Math.abs(c.sh - 224) < 0.5, 'the kit\'s cover: the whole picture');
c = h.coverCut(400, 600, 'auto');
check(c.w === 112 && c.h === 158 && Math.abs(c.sx) < 1e-6 && c.sy > 0, 'a tall picture makes a tall box, cut top and bottom');
c = h.coverCut(1000, 500, 'wide');
check(c.w === 158 && Math.abs(c.sy) < 1e-6 && c.sx > 0, 'a very wide picture is cut at the sides');

console.log('.zip');
const crcTable = new Uint32Array(256).map((_, n) => { let x = n; for (let k = 0; k < 8; k++) x = x & 1 ? 0xEDB88320 ^ (x >>> 1) : x >>> 1; return x >>> 0; });
const crc32 = bytes => { let x = 0xFFFFFFFF; for (const v of bytes) x = crcTable[(x ^ v) & 0xFF] ^ (x >>> 8); return (x ^ 0xFFFFFFFF) >>> 0; };
const enc = new TextEncoder();
const zip = h.zipStore([{ path: 'menu/hacks.txt', bytes: enc.encode(kit) },
                        { path: 'menu/metadata/X/M/L/E/metadata.ini', bytes: enc.encode(ini) }], crc32, new Date(2026, 9, 23, 12, 0, 0));
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'hackzip-'));
fs.writeFileSync(path.join(tmp, 't.zip'), zip);
try {
  const out = execFileSync('python3', ['-I', '-c',
    'import sys,zipfile; z=zipfile.ZipFile(sys.argv[1]); assert z.testzip() is None; ' +
    'print("|".join(i.filename for i in z.infolist())); print(z.read("menu/hacks.txt").decode())', path.join(tmp, 't.zip')]).toString();
  check(out.split('\n')[0] === 'menu/hacks.txt|menu/metadata/X/M/L/E/metadata.ini', 'Python reads both files');
  check(out.slice(out.indexOf('\n') + 1) === kit + '\n', 'and their contents, CRCs checked');
} catch (e) {
  check(false, 'Python could not read the .zip: ' + e.message);
} finally {
  fs.rmSync(tmp, { recursive: true });
}

console.log(checks + ' checks, ' + failures + ' failed');
process.exit(failures ? 1 : 0);
