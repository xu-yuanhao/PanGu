// Usage: node tests/core/streaming-json-diff.mjs NATIVE_DRIVER build/pi-diff
import {execFileSync} from 'node:child_process';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const {ToolCallArgumentsStream}=await import(pathToFileURL(resolve(process.argv[3],'utils/streaming-json.ts')));
const vectors=['{"n":18446744073709551616}','{"n":-18446744073709551616}','{"n":1e309}','{"n":-1e309}','{"n":1e-999}','{"n":9007199254740993}','{"a":[1e309,-1e309]}','','{}','{"a":1}','{"a":"hello','{"a":tru','{"a":-1.2e+',
 '{"a":[1,2,{"b":"x"}]}','{"x":"\\u0041\\n\\q"}','{"a":1 "b" 2}',
 '{"a":1,"a":{"b":2}}','{"s":"\\ud83d\\ude00"}','{"s":"\\ud800"}',
 '{"\\ud800":1,"\\ufffd":2}', '{"\\udc00":{"\\ud800":3}}', '{"\\ud83d\\ude00":4}',
 '{"bad":wat,"ok":true}','{"raw":"a\nb"}','[1,2]','{"a":1} trailing',
 // Pi streaming-json.test.ts string/member and malformed-text regressions.
 String.raw`{"path":"a.tex","content":"\usepackage{x}","mode":"w"}`,
 String.raw`{"text":"caf\u00e9 \url"}`,String.raw`{"text":"ab\u00`,
 '{"__proto__":{"polluted":true},"b":{"__proto__":"x"}}',
 '{"a":1,"b":2,"a":"three"}','{"code":"x\t\t  ',
 '{"a":1 "b":2}','{"a" 1, "b": 2}',"{a:1, 'b': 2}",'{"a":1,,"b":2,}',
 '[1 2, , 3,]','{"a":[1,2}, "b":3]',']]]}}}{',
 '{"a":NaN,"b":Infinity,"c":-Infinity,"d":undefined,"e":1}',
 '{"a":01,"b":1.,"c":.5,"d":+1,"e":0x10,"f":1e}',
 '{"a":"x"} trailing garbage {"b":2}','{"a": tru, "b": nulll, "c": falsey}',
 '{"a":{"b":{"c":','{"a\\": 1}','"unterminated \\',
 '// comment\n{"a":1}','\u0000{\u0001"a"\u0002:\u00031}','{"a":"x"}}}]]]'];
for(const cp of [9,10,11,12,13,32,0xa0,0x1680,...Array.from({length:11},(_,i)=>0x2000+i),0x2028,0x2029,0x202f,0x205f,0x3000,0xfeff])
 vectors.push(String.fromCodePoint(cp));
let failed=0;
for(const input of vectors) {
 const stream=new ToolCallArgumentsStream(),expected=[];
 for(const char of input)expected.push(JSON.parse(JSON.stringify(stream.append(char))));
 const final=stream.finish();expected.push(JSON.parse(JSON.stringify({arguments:final.arguments,hasError:!!final.argumentsError})));
 const output=execFileSync(resolve(process.argv[2]),[],{input,encoding:'utf8'}).trim();
 const byteSnapshots=output.split('\n').map(line=>JSON.parse(line));
 // C consumes UTF-8 bytes; compare snapshots at matching complete code-point
 // boundaries, not JavaScript characters against each partial UTF-8 byte.
 let offset=0;const native=[];
 for(const char of input){offset+=Buffer.byteLength(char);native.push(byteSnapshots[offset-1]);}
 native.push(byteSnapshots.at(-1));
 try{assert.deepEqual(native,expected);}catch(error){++failed;console.error(`DIFF ${JSON.stringify(input)}: ${error.message}`);}
}
if(failed)throw Error(`${failed}/${vectors.length} vectors differ`);
console.log(`identical previews and final argument/error outcomes for ${vectors.length} vectors`);
// Same fixed corpus consumed by the adopted upstream streaming-json.test.ts.
const suite=JSON.parse(readFileSync(new URL('../../references/pi/packages/ai/test/data/json-test-suite.json',import.meta.url),'utf8'));
let corpusFailures=0;
for(const [name,base64] of Object.entries(suite.files)) {
 const input=Buffer.from(base64,'base64').toString('utf8');
 const stream=new ToolCallArgumentsStream();stream.append(input);
 const result=stream.finish();
 const expected=JSON.parse(JSON.stringify({arguments:result.arguments,hasError:!!result.argumentsError}));
 const native=JSON.parse(execFileSync(resolve(process.argv[2]),['--final'],{input,encoding:'utf8'}));
 try{assert.deepEqual(native,expected);}catch(error){++corpusFailures;console.error(`CORPUS ${name}: ${error.message}`);}
}
if(corpusFailures)throw Error(`${corpusFailures}/${Object.keys(suite.files).length} corpus final outcomes differ`);
console.log(`identical strict tool argument/error outcomes for ${Object.keys(suite.files).length} upstream JSONTestSuite documents`);
