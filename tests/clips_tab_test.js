// PC-side tests for the Clips tab of docs/companion/index.html: clip.txt,
// frame delays, the palette, and the GIF itself. Writes GIFs to the folder
// given for tests/clips_gif_check.py to decode with Pillow. The code under
// test is taken straight out of the page. Run with tests/run.sh.
'use strict';
const fs = require('fs');
const path = require('path');

const html = fs.readFileSync(path.join(__dirname, '..', 'docs', 'companion', 'index.html'), 'utf8');
const a = html.indexOf('// --- gif logic'), b = html.indexOf('// --- end gif logic ---');
if (a < 0 || b < 0) throw new Error('Could not find the logic block in the Clips tab; did it change?');
const t = new Function(html.slice(a, b) + 'return {parseClipTxt, gifDelays, medianCut, makeMapper, lzwEncode, GifWriter};')();
const out = process.argv[2] || '.';

let checks = 0, failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL: ' + what); }
}

console.log('clip.txt and timing');
const info = t.parseClipTxt('clip 1\nfps 15\nframes 75\nsize 320x240\n');
check(info && info.fps === 15 && info.frames === 75 && info.size === '320x240', 'as clip.c writes it');
check(t.parseClipTxt('hidden 1\n') === null, 'another file is not taken for it');
const d15 = t.gifDelays(15, 15), d30 = t.gifDelays(30, 30);
check(d15.reduce((x, y) => x + y, 0) === 100 && d15.every(d => d === 6 || d === 7), '15 a second: 6s and 7s adding up to one second');
check(d30.reduce((x, y) => x + y, 0) === 100 && d30.every(d => d === 3 || d === 4), '30 a second: 3s and 4s');

console.log('Palette');
const samples = [];
for (let i = 0; i < 40; i++) samples.push(i * 6, 255 - i * 6, (i * 37) % 256);
const pal = t.medianCut(samples, 255);
check(pal.length === 40, 'no more colors than the picture has');
const map = t.makeMapper(pal);
check(pal[map(samples[30], samples[31], samples[32])].join() === [samples[30], samples[31], samples[32]].join(), 'a color finds itself');

// Frames: a few colors, a square that moves, and one frame that doesn't change.
(async () => {
  const W = 64, H = 48, colors = [[0, 0, 0], [255, 0, 0], [0, 128, 255], [250, 250, 250]];
  function frame(n) {
    const idx = new Uint8Array(W * H);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) idx[y * W + x] = (x < 32 ? 2 : 3);
    for (let y = 10; y < 20; y++) for (let x = n * 4; x < n * 4 + 10; x++) idx[y * W + x] = 1;
    return idx;
  }
  const g = t.GifWriter(W, H, colors, true);
  const seq = [0, 1, 1, 2, 3];
  seq.forEach((n, i) => g.add(frame(n), 7));
  fs.writeFileSync(path.join(out, 'moving.gif'), Buffer.from(await g.finish().arrayBuffer()));
  const raw = [];
  seq.forEach(n => raw.push(...Array.from(frame(n)).map(i => colors[i]).flat()));
  fs.writeFileSync(path.join(out, 'moving.raw'), Buffer.from(raw));

  // Noise: enough codes to fill LZW's table and start it again.
  const NW = 320, NH = 240, noise = new Uint8Array(NW * NH), npal = [];
  for (let i = 0; i < 255; i++) npal.push([i, (i * 7) % 256, (i * 13) % 256]);
  let s = 1;
  for (let i = 0; i < noise.length; i++) { s = (s * 1103515245 + 12345) >>> 0; noise[i] = (s >>> 24) % 255; }
  const n = t.GifWriter(NW, NH, npal, false);
  n.add(noise, 10);
  fs.writeFileSync(path.join(out, 'noise.gif'), Buffer.from(await n.finish().arrayBuffer()));
  fs.writeFileSync(path.join(out, 'noise.raw'), Buffer.from(Array.from(noise).map(i => npal[i]).flat()));

  check(fs.statSync(path.join(out, 'moving.gif')).size < 1400, 'unchanged parts of a frame cost almost nothing (the palette alone is 768 bytes)');
  console.log(failures ? failures + ' of ' + checks + ' checks failed.' : checks + ' checks, 0 failed');
  process.exit(failures ? 1 : 0);
})();
