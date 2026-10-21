// PC-side tests for the Card Prep page (prep-tool/prep-tool.html). The code
// under test is taken straight out of the page. Run with tests/run.sh.
'use strict';
const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'prep-tool', 'prep-tool.html'), 'utf8');
const a = html.indexOf('// --- logic'), b = html.indexOf('// --- end logic ---');
if (a < 0 || b < 0) throw new Error('Could not find the logic block in the Card Prep page; did it change?');
const pt = new Function(html.slice(a, b) +
  'return {crcAdd, crcEnd, hex8, byteOrder, toZ64, gameCode, pathKey, parseDat, classify, formatLine, parseVerified, isRom, skipFolder};')();

let checks = 0, failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL: ' + what); }
}

console.log('CRC-32');
const nine = new TextEncoder().encode('123456789');
check(pt.hex8(pt.crcEnd(pt.crcAdd(0xFFFFFFFF, nine))) === 'cbf43926', 'the standard check value');
let pieces = pt.crcAdd(0xFFFFFFFF, nine.slice(0, 4));
pieces = pt.crcAdd(pieces, nine.slice(4));
check(pt.hex8(pt.crcEnd(pieces)) === 'cbf43926', 'the same when given in two pieces');

console.log('Byte orders');
const z64 = new Uint8Array(64);
z64.set([0x80, 0x37, 0x12, 0x40]);
z64.set([0x4E, 0x53, 0x4D, 0x45], 0x3B);   // NSME
const v64 = new Uint8Array(64), n64 = new Uint8Array(64);
for (let i = 0; i < 64; i += 2) { v64[i] = z64[i + 1]; v64[i + 1] = z64[i]; }
for (let i = 0; i < 64; i += 4) for (let k = 0; k < 4; k++) n64[i + k] = z64[i + 3 - k];
check(pt.byteOrder(z64) === 'z64' && pt.byteOrder(v64) === 'v64' && pt.byteOrder(n64) === 'n64', 'all three are told apart');
check(pt.byteOrder(new Uint8Array(64)) === null, 'a file that is not a dump is not taken for one');
check(Buffer.compare(Buffer.from(pt.toZ64(v64.slice(), 'v64')), Buffer.from(z64)) === 0, 'v64 is put in z64 order');
check(Buffer.compare(Buffer.from(pt.toZ64(n64.slice(), 'n64')), Buffer.from(z64)) === 0, 'n64 is put in z64 order');
check(pt.gameCode(z64) === 'NSME', 'the game code is read from the header');

console.log('Path key (must match path_key() in game_index.c)');
// FNV-1a, worked out separately: Python's sum over the same bytes.
check(pt.hex8(pt.pathKey('sd:/N64(US)/Super Mario 64 (U) [!].v64')) === 'fa2c160d', 'the key of a real path (' + pt.hex8(pt.pathKey('sd:/N64(US)/Super Mario 64 (U) [!].v64')) + ')');

console.log('No-Intro list');
const dat = `clrmamepro ( name "Nintendo - Nintendo 64" )
game (
	name "Super Mario 64 (USA)"
	serial "NSME"
	rom ( name "Super Mario 64 (USA).z64" size 8388608 crc 3CE60709 md5 X sha1 Y serial "NSME" )
)
game (
	name "Super Mario 64 (USA)"
	serial "NSME"
	rom ( name "Super Mario 64 (USA).v64" size 8388608 crc 11111111 serial "NSME" )
)`;
const db = pt.parseDat(dat);
check(db.byCrc.size === 1, 'only the z64 dumps are kept (' + db.byCrc.size + ')');
check(db.byCrc.get('3ce60709') && db.byCrc.get('3ce60709').name === 'Super Mario 64 (USA)', 'name without the extension');
check(db.codes.has('NSME'), 'the game code is known');
check(pt.classify(0x3CE60709, 'NSME', db) === 'V', 'a listed dump is verified');
check(pt.classify(0x12345678, 'NSME', db) === 'M', 'a known game with another CRC is not a known dump');
check(pt.classify(0x12345678, 'ZZZZ', db) === 'U', 'an unknown game is not in the list');

console.log('verified.txt');
const line = pt.formatLine({ key: 0xABCDEF01, size: 8388608, crc: 0x3CE60709, result: 'V', known: 'Super Mario 64 (USA)' });
check(line === 'abcdef01 8388608 3ce60709 V Super Mario 64 (USA)', 'line layout (' + line + ')');
const back = pt.parseVerified('verified 1\n' + line + '\nabcdef02 100 00000001 U\n');
check(back.size === 2 && back.get('abcdef01 8388608').crc === 0x3CE60709, 'lines read back');
check(pt.parseVerified('something else\n' + line).size === 0, 'a file with another header is ignored');

console.log('Which files and folders');
check(pt.isRom('Game.Z64') && pt.isRom('a.v64') && pt.isRom('b.n64') && !pt.isRom('c.zip') && !pt.isRom('z64'), 'ROM extensions');
check(pt.skipFolder('menu') && pt.skipFolder('Saves') && pt.skipFolder('.Trashes') && !pt.skipFolder('N64(US)'), 'folders left out');

console.log(checks + ' checks, ' + failures + ' failed');
process.exit(failures ? 1 : 0);
