// PC-side tests for the Profile Pictures tab of docs/companion/index.html: the
// file names it writes must be ones the console lists (src/menu/avatars.c),
// and the square it takes must stay inside the picture. The code under test is
// taken straight out of the page. Run with tests/run.sh.
'use strict';
const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'docs', 'companion', 'index.html'), 'utf8');
const a = html.indexOf('// --- pictures logic'), b = html.indexOf('// --- end pictures logic ---');
if (a < 0 || b < 0) throw new Error('Could not find the logic block in the Profile Pictures tab; did it change?');
const t = new Function(html.slice(a, b) + 'return {picFileName, picUniqueNames, picSquare, PIC_SIDE};')();

let checks = 0, failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL: ' + what); }
}

console.log('file names');
check(t.picFileName('Mario.jpg') === 'Mario', 'the extension is left off');
check(t.picFileName('Link: Hero?.png') === 'Link Hero', 'characters a card can\'t hold are dropped');
check(t.picFileName('  Star. .webp') === 'Star', 'no trailing dot or space');
check(t.picFileName('') === 'Picture' && t.picFileName('.png') === 'Picture' && t.picFileName('.hidden') === 'Picture', 'an empty or hidden name gets a plain one');
const long = t.picFileName('x'.repeat(80));
check(long.length === 40 && (long + '.png').length < 45, 'long names fit the console\'s 44 characters with .png');
check(t.picFileName('Mew Bubble 2') === 'Mew Bubble 2', 'a name without an extension is kept');

console.log('names made different');
const u = t.picUniqueNames(['Mew', 'mew', 'Mew', 'Star']);
check(u[0] === 'Mew' && u[1] === 'mew 2' && u[2] === 'Mew 3' && u[3] === 'Star', 'repeats numbered, ignoring case: ' + u.join(', '));
const lu = t.picUniqueNames(['y'.repeat(40), 'y'.repeat(40)]);
check(lu[1].length <= 40 && lu[1] !== lu[0], 'a numbered long name still fits');

console.log('the square');
let s = t.picSquare(800, 600, 1, 400, 300);
check(s.side === 600 && s.x === 100 && s.y === 0, 'zoom 1: the largest square, in the middle');
s = t.picSquare(800, 600, 0.5, 0, 0);
check(s.side === 300 && s.x === 0 && s.y === 0, 'dragged past the corner: held inside');
s = t.picSquare(800, 600, 0.5, 9999, 9999);
check(s.x === 500 && s.y === 300, 'dragged past the far corner: held inside');
s = t.picSquare(10, 10, 0, 5, 5);
check(s.side === 1, 'zoom is held at a tenth');
check(t.PIC_SIDE === 64, 'pictures are 64 x 64, as on the console');

console.log(`${checks - failures} of ${checks} checks passed`);
process.exit(failures ? 1 : 0);
