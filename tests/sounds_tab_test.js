// PC-side tests for the Sounds tab of docs/companion/index.html: the sound
// files it writes must be exactly what libdragon's audioconv64 writes for
// the same samples (--wav-compress 0), which the console's theme_pack.c
// checks and wav64_open() plays. The code under test is taken straight out
// of the page. Run with tests/run.sh (the comparison needs audioconv64).
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const html = fs.readFileSync(path.join(__dirname, '..', 'docs', 'companion', 'index.html'), 'utf8');
const a = html.indexOf('// --- sounds logic'), b = html.indexOf('// --- end sounds logic ---');
if (a < 0 || b < 0) throw new Error('Could not find the logic block in the Sounds tab; did it change?');
const t = new Function(html.slice(a, b) + 'return {wav64Raw, fadeTail, themeFolderName, SND_SLOTS, SND_RATE};')();

let checks = 0, failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL: ' + what); }
}

// A plain 16-bit WAV of the given integer samples, for audioconv64.
function wavFile(chans, rate) {
  const ch = chans.length, n = chans[0].length, out = Buffer.alloc(44 + n * ch * 2);
  out.write('RIFF', 0); out.writeUInt32LE(36 + n * ch * 2, 4); out.write('WAVE', 8);
  out.write('fmt ', 12); out.writeUInt32LE(16, 16); out.writeUInt16LE(1, 20); out.writeUInt16LE(ch, 22);
  out.writeUInt32LE(rate, 24); out.writeUInt32LE(rate * ch * 2, 28); out.writeUInt16LE(ch * 2, 32); out.writeUInt16LE(16, 34);
  out.write('data', 36); out.writeUInt32LE(n * ch * 2, 40);
  for (let i = 0; i < n; i++) for (let c = 0; c < ch; c++) out.writeInt16LE(chans[c][i], 44 + (i * ch + c) * 2);
  return out;
}

console.log('the file the console reads');
const tone = (n, f) => Array.from({ length: n }, (_, i) => Math.round(12000 * Math.sin(i * f)));
const mono = [tone(1001, 0.05)];
const stereo = [tone(777, 0.05), tone(777, 0.1)];
const asFloat = (ch) => ch.map((d) => Float32Array.from(d, (v) => v / 32767));
const m = t.wav64Raw(asFloat(mono), 22050);
check(Buffer.from(m.slice(0, 4)).toString() === 'WV64' && m[4] === 10 && m[5] === 0 && m[6] === 1 && m[7] === 16, 'header id, version 10, raw, mono, 16 bits');
check(m.length === 32 + 1001 * 2, 'mono size');
const dv = new DataView(m.buffer);
check(dv.getInt32(8) === 22050 && dv.getInt32(12) === 1001 && dv.getUint32(24) === 32 && dv.getUint32(28) === 0, 'rate, length, start, state');
check(dv.getInt16(32) === mono[0][0] && dv.getInt16(34) === mono[0][1], 'samples big-endian');
const clip = t.wav64Raw([Float32Array.from([2, -2, 0.5])], 22050);
const cv = new DataView(clip.buffer);
check(cv.getInt16(32) === 32767 && cv.getInt16(34) === -32767 && cv.getInt16(36) === 16384, 'loud samples clipped, not wrapped');

const conv = path.join(process.env.N64_INST || '/opt/libdragon', 'bin', 'audioconv64');
if (fs.existsSync(conv)) {
  console.log('the same as audioconv64');
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'snd-'));
  for (const [name, chans] of [['mono', mono], ['stereo', stereo]]) {
    fs.writeFileSync(path.join(dir, name + '.wav'), wavFile(chans, 22050));
    execFileSync(conv, ['--wav-compress', '0', '-o', dir, path.join(dir, name + '.wav')], { stdio: 'ignore' });
    const ref = fs.readFileSync(path.join(dir, name + '.wav64'));
    const ours = Buffer.from(t.wav64Raw(asFloat(chans), 22050));
    check(ref.equals(ours), name + ': byte for byte like audioconv64 (' + ref.length + ' bytes)');
  }
  fs.rmSync(dir, { recursive: true, force: true });
} else {
  console.log('audioconv64 not found: comparison skipped');
}

console.log('cutting and names');
const tail = t.fadeTail([new Float32Array(100).fill(1)], 1000, 10);
check(tail[0][89] === 1 && tail[0][99] === 0 && tail[0][95] > 0 && tail[0][95] < 1, 'the last 10 ms fade to silence');
check(t.themeFolderName(' Lava/Lamp: 2 ') === 'LavaLamp 2', 'slashes and colons left out of the folder name');
check(t.SND_SLOTS.every((s) => s.rate <= 44100 && s.rate >= 8000), 'every rate is one src/menu/theme_pack.c accepts (8000-44100)');
check(t.SND_SLOTS.filter((s) => s.channels === 1).every((s) => s.seconds * s.rate * 2 + 32 <= 128 * 1024), 'a button sound at its longest fits the 8MB limit');
check(t.SND_SLOTS.map((s) => s.file).join() === 'music.wav64,intro.wav64,move.wav64,select.wav64,back.wav64,setting.wav64,error.wav64',
  'the file names src/menu/theme_pack.h reads');

console.log(checks + ' checks, ' + failures + ' failed');
process.exit(failures ? 1 : 0);
