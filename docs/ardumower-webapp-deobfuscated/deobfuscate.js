const fs = require('fs');
const path = require('path');

function deobfuscate(inputFile, outputFile) {
  let code = fs.readFileSync(inputFile, 'utf8');

  // Locate the literal table: var a0_0x... = ['str1', 'str2', ...];
  const m = code.match(/var (a0_0x[0-9a-f]+) = \[([^\]]+)\];/s);
  if (!m) {
    console.error('Literal table not found in', inputFile);
    return;
  }
  const tableName = m[1];
  const strings = m[2].match(/'([^']+)'/g).map(s => s.slice(1, -1));

  // Locate the accessor function: var a0_0x... = function(_0x..., _0x...) { ... tableName[ ... }
  const fnRe = new RegExp('var (a0_0x[0-9a-f]+) = function\\(_0x[0-9a-f]+, _0x[0-9a-f]+\\) \\{[^}]+' + tableName + '\\[', 's');
  const fnm = code.match(fnRe);
  if (!fnm) {
    console.error('Accessor function not found in', inputFile);
    return;
  }
  const fnName = fnm[1];

  const callRe = new RegExp(fnName + "\\('0x([0-9a-f]+)'\\)", 'g');
  code = code.replace(callRe, (_, idx) => JSON.stringify(strings[parseInt(idx, 16)]));

  fs.writeFileSync(outputFile, code);
  console.log('Wrote', outputFile, '(replaced', strings.length, 'strings)');
}

if (process.argv.length < 3) {
  console.log('Usage: node deobfuscate.js <input.js> [output.js]');
  process.exit(1);
}

const input = process.argv[2];
const output = process.argv[3] || input.replace(/\.js$/, '.deobf.js');
deobfuscate(input, output);
