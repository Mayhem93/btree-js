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
const bogusKeys = new Uint32Array(MAX / 4);

for (let i = 0; i < MAX; i++) {
  // use Math.floor; avoids string parsing
  keys[i] = Math.floor(Math.random() * MAX * MAX);
  values[i] = 'something';
}

for (let i = 0; i < MAX / 4; i++) {
  // use Math.floor; avoids string parsing
  bogusKeys[i] = Math.floor(Math.random() * (MAX / 4)**2 );
}

// 2) Warm up a bit (optional)
for (let i = 0; i < 10000; i++) {
  Math.floor(Math.random() * 1000);
}

const tree = new BTreeJS();

// 3) Benchmark B-Tree insert
{
  logMemory('[BTREE] Before');

  const t0 = process.hrtime.bigint();

  for (let i = 0; i < MAX; i++) {
    tree.insert(keys[i], values[i]);
  }
  const t1 = process.hrtime.bigint();

  console.log(`BTree insert: ${((t1 - t0) / 1000000n)} ms`);
  logMemory('[BTREE] After');
}

const mixedKeys = new Uint32Array(keys.length + bogusKeys.length);

mixedKeys.set(keys);
mixedKeys.set(bogusKeys, keys.length);

{
  const t0 = process.hrtime.bigint();

  for (let i = 0; i < mixedKeys.length; i++) {
    tree.search(mixedKeys[i]);
  }
  const t1 = process.hrtime.bigint();

  console.log(`BTree search: ${((t1 - t0) / 1000000n)} ms`);
}

// tree.move(mixedKeys[0], mixedKeys[1]);

// 4) Benchmark Map insert + in-place sort
{
  let map = new Map();

  logMemory('[MAP()] Before');

  const t0 = process.hrtime.bigint();

  for (let i = 0; i < MAX; i++) {
    map.set(keys[i], values[i]);
  }

  // convert to array and sort by numeric key
  const arr = Array.from(map.entries());

  arr.sort((a, b) => a[0] - b[0]);
  const t1 = process.hrtime.bigint();

  map = new Map(arr);

  for (let k of mixedKeys) {
    map.get(k);
  }

  console.log(`Map+sort+lookup:     ${((t1 - t0) / 1000000n)} ms`);
  logMemory('[MAP()] After');
}
