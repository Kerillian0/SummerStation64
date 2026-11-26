// VADPCM encoder for the SummerStation64 Companion: turns 16-bit samples into
// a compressed wav64 file the console keeps compressed in memory (about a
// quarter of the size) and the sound chip unpacks as it plays.
//
// A JavaScript port of the VADPCM codec by Dietrich Epp (Copyright 2022-2023
// Dietrich Epp), as included in libdragon's tools/audioconv64/vadpcm/codec
// (autocorr.c, predictor.c, predictor.h, random.h, encode.c), and of the
// wav64 writing in libdragon's tools/audioconv64/conv_wav64.cpp for
// `--wav-compress vadpcm,huffman=false` with no loop and no seek points.
//
// This file is licensed under the terms of the Mozilla Public License,
// version 2.0, like the code it is ported from: http://mozilla.org/MPL/2.0/
// The source this was ported from: https://github.com/DragonMinded/libdragon
// (tools/audioconv64/vadpcm, itself from https://github.com/depp/vadpcm).
//
// The original is built with strict IEEE arithmetic (-ffp-contract=off
// -fno-fast-math): single-precision steps are rounded with Math.fround here,
// so the output is the same byte for byte (tests/sounds_tab_test.js checks
// it against audioconv64).
(function (root) {
  'use strict';
  var f = Math.fround;
  var FRAME = 16, FRAME_BYTES = 9, ORDER = 2, PREDICTORS = 4, ITERATIONS = 20;

  function rng(state) { return (Math.imul(state, 0xd9f5) + 0x6487ed51) >>> 0; }

  // C's lrint in the default rounding mode: to nearest, ties to even.
  function lrint(x) {
    var r = Math.round(x);
    if (r - x === 0.5 && (r % 2) !== 0) r -= 1;
    return r;
  }

  // autocorr.c: per frame, six single-precision sums.
  function autocorr(frames, src) {
    var corr = new Array(frames), scale = f(1 / 32768);
    var x0 = 0, x1 = 0, x2 = 0;
    for (var fr = 0; fr < frames; fr++) {
      var m0 = 0, m1 = 0, m2 = 0, m3 = 0, m4 = 0, m5 = 0;
      for (var i = 0; i < FRAME; i++) {
        x2 = x1; x1 = x0;
        x0 = f(src[fr * FRAME + i] * scale);
        m0 = f(m0 + f(x0 * x0));
        m1 = f(m1 + f(x1 * x0));
        m2 = f(m2 + f(x1 * x1));
        m3 = f(m3 + f(x2 * x0));
        m4 = f(m4 + f(x2 * x1));
        m5 = f(m5 + f(x2 * x2));
      }
      corr[fr] = [m0, m1, m2, m3, m4, m5];
    }
    return corr;
  }

  // predictor.h: the square error for single-precision coefficients.
  function evalF(c, k0, k1) {
    var a = f(c[0] + f(f(c[2] * k0) * k0));
    a = f(a + f(f(c[5] * k1) * k1));
    var inner = f(f(f(c[4] * k0) * k1) - f(c[1] * k0));
    inner = f(inner - f(c[3] * k1));
    return f(a + f(2 * inner));
  }
  function evalSolved(c, k) { return c[0] - c[1] * k[0] - c[3] * k[1]; }
  function evalAt(c, c0, c1) {
    return c[0] + c[2] * c0 * c0 + c[5] * c1 * c1 + 2.0 * (c[4] * c0 * c1 - c[1] * c0 - c[3] * c1);
  }

  // predictor.c
  function solve(c, k) {
    var relEpsilon = 1.0e-5;
    k[0] = 0; k[1] = 0;
    var max = c[0];
    if (c[2] > max) max = c[2];
    if (c[5] > max) max = c[5];
    var epsilon = max * relEpsilon;
    var a = c[2], b = c[4], cc = c[5], x = c[1], y = c[3];
    var pivot = cc > a ? 1 : 0;
    if (pivot) { var t = a; a = cc; cc = t; t = x; x = y; y = t; }
    if (a <= epsilon) return;
    var a1 = 1.0 / a, b1 = b * a1, x1 = x * a1;
    var c2 = cc - b1 * b, y2 = y - x1 * b;
    if (Math.abs(c2) <= epsilon) { k[pivot] = x1; return; }
    var y3 = y2 / c2, x4 = x1 - y3 * b1;
    k[pivot] = x4;
    k[pivot ? 0 : 1] = y3;
  }
  function solveEdge(c, x0, y0, x1, y1, best, k) {
    var vx = x1 - x0, vy = y1 - y0;
    var apx = c[2] * x0 + c[4] * y0, apy = c[4] * x0 + c[5] * y0;
    var num = vx * (c[1] - apx) + vy * (c[3] - apy);
    var den = vx * (c[2] * vx + c[4] * vy) + vy * (c[4] * vx + c[5] * vy);
    var t;
    if (den > 0.0) { t = num / den; if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0; }
    else t = num > 0.0 ? 1.0 : 0.0;
    var c0 = x0 + t * vx, c1 = y0 + t * vy, error = evalAt(c, c0, c1);
    if (error < best.v) { best.v = error; k[0] = c0; k[1] = c1; }
  }
  function solveStable(c, k) {
    solve(c, k);
    if (k[1] >= -1.0 && k[0] + k[1] <= 1.0 && k[1] - k[0] <= 1.0) return 0;
    var best = { v: Number.MAX_VALUE };
    k[0] = 0; k[1] = 0;
    solveEdge(c, -2.0, -1.0, 2.0, -1.0, best, k);
    solveEdge(c, 2.0, -1.0, 0.0, 1.0, best, k);
    solveEdge(c, 0.0, 1.0, -2.0, -1.0, best, k);
    return 1;
  }
  function bestError(corr) {
    var out = new Array(corr.length);
    for (var fr = 0; fr < corr.length; fr++) {
      var k = [0, 0];
      if (solveStable(corr[fr], k) === 0) out[fr] = f(evalSolved(corr[fr], k));
      else out[fr] = evalF(corr[fr], f(k[0]), f(k[1]));
    }
    return out;
  }
  function meancorrs(corr, count, predictors) {
    var p = [], n = [];
    for (var i = 0; i < count; i++) { n.push(0); p.push([0, 0, 0, 0, 0, 0]); }
    for (var fr = 0; fr < corr.length; fr++) {
      var q = predictors[fr];
      if (q < count) { n[q]++; for (var j = 0; j < 6; j++) p[q][j] += corr[fr][j]; }
    }
    for (i = 0; i < count; i++) {
      if (n[i] > 0) { var a = 1.0 / n[i]; for (j = 0; j < 6; j++) p[i][j] *= a; }
    }
    return { pcorr: p, count: n };
  }
  function refine(corr, count, error, predictors) {
    var mc = meancorrs(corr, count, predictors), coeff = [], active = 0, i;
    for (i = 0; i < count; i++) {
      if (mc.count[i] > 0) {
        var d = [0, 0];
        solveStable(mc.pcorr[i], d);
        coeff[active++] = [f(d[0]), f(d[1])];
      }
    }
    var count2 = new Array(active).fill(0);
    for (var fr = 0; fr < corr.length; fr++) {
      var fp = 0, fe = 0;
      for (i = 0; i < active; i++) {
        var e = evalF(corr[fr], coeff[i][0], coeff[i][1]);
        if (i === 0 || e < fe) { fp = i; fe = e; }
      }
      predictors[fr] = fp;
      error[fr] = fe;
      count2[fp]++;
    }
    for (i = 0; i < active; i++) if (count2[i] === 0) return i;
    return active;
  }
  function worstFrame(best, error) {
    var bi = f(error[0] - best[0]), at = 0;
    for (var fr = 1; fr < best.length; fr++) {
      var imp = f(error[fr] - best[fr]);
      if (imp > bi) { bi = imp; at = fr; }
    }
    return at;
  }
  function assignPredictors(corr, count) {
    var predictors = new Uint8Array(corr.length);
    if (count <= 1) return predictors;
    var best = bestError(corr), error = new Array(corr.length).fill(0);
    var unassigned = count, active = 1;
    for (var iter = 0; iter < ITERATIONS; iter++) {
      if (unassigned < count) {
        var worst = worstFrame(best, error);
        predictors[worst] = unassigned;
        if (unassigned >= active) active = unassigned + 1;
      }
      unassigned = refine(corr, active, error, predictors);
    }
    return predictors;
  }

  // encode.c
  function makeVectors(k) {
    var scale = 2048, vec = [];
    for (var i = 0; i < 2; i++) {
      var x1 = 0.0, x2 = 0.0, v = [];
      if (i === 0) x2 = scale; else x1 = scale;
      for (var j = 0; j < 8; j++) {
        var x = k[0] * x1 + k[1] * x2;
        v.push(x > 0x7fff ? 0x7fff : (x < -0x8000 ? -0x8000 : lrint(x)));
        x2 = x1; x1 = x;
      }
      vec.push(v);
    }
    return vec;
  }
  function makeCodebook(corr, count, predictors) {
    var mc = meancorrs(corr, count, predictors), book = [];
    for (var i = 0; i < count; i++) {
      if (mc.count[i] > 0) {
        var k = [0, 0];
        solveStable(mc.pcorr[i], k);
        var v = makeVectors(k);
        book.push(v[0], v[1]);
      } else {
        book.push([0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0]);
      }
    }
    return book;
  }
  function getshift(min, max, minR, maxR) {
    var shift = 0;
    while (shift < 12 && (min < minR || maxR < max)) { min >>= 1; max >>= 1; shift++; }
    return shift;
  }
  // Dithered (rectangular), residuals -8..7: what audioconv64 uses.
  function encodeData(frames, src, predictors, book) {
    var minR = -8, maxR = 7, dest = new Uint8Array(frames * FRAME_BYTES);
    var rngState = 0, state = [0, 0, 0, 0], acc = new Int32Array(8), fout = new Uint8Array(8);
    for (var fr = 0; fr < frames; fr++) {
      var p = predictors[fr], v0 = book[2 * p], v1 = book[2 * p + 1];
      var base = fr * 16, s0, s1, s, a, r, i, j, vector;
      state[2] = src[base + 6];
      state[3] = src[base + 7];
      var min = 0, max = 0;
      for (vector = 0; vector < 2; vector++) {
        s0 = state[vector * 2]; s1 = state[vector * 2 + 1];
        for (i = 0; i < 8; i++) {
          acc[i] = (Math.imul(src[base + vector * 8 + i], 2048) - Math.imul(s0, v0[i]) - Math.imul(s1, v1[i])) | 0;
        }
        for (i = 0; i < 8; i++) {
          s = acc[i] >> 11;
          if (s < min) min = s;
          if (s > max) max = s;
          for (j = 0; j < 7 - i; j++) acc[i + 1 + j] = (acc[i + 1 + j] - Math.imul(s, v1[j])) | 0;
        }
      }
      var shift = getshift(min, max, minR, maxR);
      var bestErr = 0.0, minShift = shift > 0 ? shift - 1 : 0, maxShift = shift < 12 ? shift + 1 : 12;
      var initState = rngState;
      for (shift = minShift; shift <= maxShift; shift++) {
        rngState = initState;
        var error = 0.0;
        s0 = state[0]; s1 = state[1];
        for (vector = 0; vector < 2; vector++) {
          for (i = 0; i < 8; i++) acc[i] = (Math.imul(s0, v0[i]) + Math.imul(s1, v1[i])) | 0;
          for (i = 0; i < 8; i++) {
            s = src[base + vector * 8 + i];
            a = acc[i] >> 11;
            var bias = (rngState >>> 16) >>> (16 - shift);
            rngState = rng(rngState);
            r = (s - a + bias) >> shift;
            if (r > maxR) r = maxR; else if (r < minR) r = minR;
            acc[i] = r;
            var sout = r * (1 << shift);
            for (j = 0; j < 7 - i; j++) acc[i + 1 + j] = (acc[i + 1 + j] + Math.imul(sout, v1[j])) | 0;
            sout += a;
            if (sout > 0x7fff) sout = 0x7fff; else if (sout < -0x8000) sout = -0x8000;
            s0 = s1; s1 = sout;
            var se = s - sout;
            error += se * se;
          }
          for (i = 0; i < 4; i++) fout[vector * 4 + i] = ((acc[2 * i] & 15) << 4) | (acc[2 * i + 1] & 15);
        }
        if (shift === minShift || error < bestErr) {
          dest[fr * 9] = (shift << 4) | p;
          dest.set(fout, fr * 9 + 1);
          state[2] = s0; state[3] = s1;
          bestErr = error;
        }
      }
      state[0] = state[2]; state[1] = state[3];
    }
    return dest;
  }

  function encodeChannel(src, frames) {
    var corr = autocorr(frames, src);
    var predictors = assignPredictors(corr, PREDICTORS);
    var book = makeCodebook(corr, PREDICTORS, predictors);
    return { book: book, data: encodeData(frames, src, predictors, book) };
  }

  // conv_wav64.cpp: the wav64 file. `channels` is an array of Int16Arrays
  // of equal length (one for mono, two for stereo).
  function wav64(channels, rate) {
    var ch = channels.length, cnt = channels[0].length;
    // Pad to a multiple of 32 samples by holding the last sample (no loop).
    var padded = Math.ceil(cnt / 32) * 32;
    var chans = channels.map(function (d) {
      var out = new Int16Array(padded);
      out.set(d);
      for (var i = cnt; i < padded; i++) out[i] = d[cnt - 1];
      return out;
    });
    var frames = padded / FRAME;
    var enc = chans.map(function (d) { return encodeChannel(d, frames); });

    var HUFF_CONTEXT_LEN = 3 * 24, CODEBOOK_STRIDE = PREDICTORS * ORDER * 16 + 8;
    var extSize = 1 + 1 + 2 + 2 + 1 + 1 + 4 + 4 + 4 + HUFF_CONTEXT_LEN + 4 + CODEBOOK_STRIDE * ch;
    var start = 32 + extSize, dataSize = frames * FRAME_BYTES * ch;
    var out = new Uint8Array(start + dataSize), v = new DataView(out.buffer);
    out.set([0x57, 0x56, 0x36, 0x34], 0);   // "WV64"
    v.setUint8(4, 10);          // version
    v.setUint8(5, 1);           // format: VADPCM
    v.setUint8(6, ch);
    v.setUint8(7, 16);          // bits
    v.setInt32(8, rate);
    v.setInt32(12, cnt);        // length in samples: the real one (the data is padded)
    v.setInt32(16, 0);          // loop length
    v.setInt32(20, 0);          // loop end
    v.setUint32(24, start);     // where the samples start
    v.setUint32(28, 48);        // state size
    var at = 32;
    v.setUint8(at++, PREDICTORS);
    v.setUint8(at++, ORDER);
    v.setUint16(at, 0); at += 2;        // flags: no Huffman, not resident
    v.setUint16(at, 0); at += 2;        // skip points
    v.setUint8(at++, 4);                // bits per residual
    v.setUint8(at++, 0);                // attack frames
    v.setUint32(at, 0); at += 4;        // huff_tbl_ptr
    v.setUint32(at, 0); at += 4;        // skip_points_ptr
    v.setUint32(at, 0); at += 4;        // skip_states_ptr
    at += HUFF_CONTEXT_LEN;             // Huffman context (zeros)
    at += 4;                            // padding
    for (var c = 0; c < ch; c++) {
      enc[c].book.forEach(function (vec) { for (var j = 0; j < 8; j++) { v.setInt16(at, vec[j]); at += 2; } });
      at += 8;                          // three loop taps (no loop) and padding
    }
    // Block-planar layout: for each block of 128 frames, all of channel 0, then channel 1.
    var B = 128, nblocks = Math.ceil(frames / B);
    for (c = 0; c < ch; c++) {
      for (var fr = 0; fr < frames; fr++) {
        var dst;
        if (ch === 1) dst = fr;
        else {
          var block = Math.floor(fr / B), off = fr % B;
          var bs = (block === nblocks - 1) ? (frames - block * B) : B;
          dst = block * B * ch + c * bs + off;
        }
        out.set(enc[c].data.subarray(fr * FRAME_BYTES, (fr + 1) * FRAME_BYTES), start + dst * FRAME_BYTES);
      }
    }
    return out;
  }

  var api = { wav64: wav64 };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.vadpcm = api;
})(typeof window !== 'undefined' ? window : this);
