import * as test from './export.js';
const BTreeJS = test.default.BTreeJs;

const MAX = 1_000_000;

function logMemory (label = '') {
  const { rss, heapUsed } = process.memoryUsage();

  console.log(
    `${label} RSS: ${(rss/1024/1024).toFixed(2)} MB, `,
    `HeapUsed: ${(heapUsed/1024/1024).toFixed(2)} MB`
  );
}

// 1) Pre-generate all keys & values
const keys = new Uint32Array(MAX);
const values = new Array(MAX);

for (let i = 0; i < MAX; i++) {
  // use Math.floor; avoids string parsing
  keys[i] = Math.floor(Math.random() * MAX * MAX);
  values[i] = 'something';
}

// 2) Warm up a bit (optional)
for (let i = 0; i < 10000; i++) {
  Math.floor(Math.random() * 1000);
}

// 3) Benchmark B-Tree insert
{
  logMemory('[BTREE] Before');
  const tree = new BTreeJS();

  const t0 = process.hrtime.bigint();

  for (let i = 0; i < MAX; i++) {
    tree.insert(keys[i], values[i]);
  }
  const t1 = process.hrtime.bigint();

  console.log(`BTree insert: ${((t1 - t0) / 1000000n)} ms`);
  logMemory('[BTREE] After');
}

// 4) Benchmark Map insert + in-place sort
{
  logMemory('[MAP()] Before');
  const map = new Map();

  const t0 = process.hrtime.bigint();

  for (let i = 0; i < MAX; i++) {
    map.set(keys[i], values[i]);
  }

  // convert to array and sort by numeric key
  const arr = Array.from(map.entries());

  arr.sort((a, b) => a[0] - b[0]);
  const t1 = process.hrtime.bigint();

  console.log(`Map+sort:     ${((t1 - t0) / 1000000n)} ms`);
  logMemory('[MAP()] After');
}
