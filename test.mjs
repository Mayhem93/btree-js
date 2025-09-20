import * as test from './export.js';

const BTreeJS = test.default.BTreeJs;

console.log(test);

const tree = new BTreeJS();

tree.insert(1, 'something');

console.log(tree.size());

console.log(tree.remove(1));

console.log(tree.size());
