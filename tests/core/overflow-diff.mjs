import {execFileSync} from 'node:child_process';
import {readFileSync,existsSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import assert from 'node:assert/strict';
const [native,pi,vectors]=process.argv.slice(2);
const source=existsSync(resolve(pi,'ai/src/utils/overflow.ts'))?'ai/src/utils/overflow.ts':'utils/overflow.ts';
const {isContextOverflow,isRecoverableLength}=await import(pathToFileURL(resolve(pi,source))); 
const expected=JSON.parse(readFileSync(vectors,'utf8')).map((v,i)=>{
 const message={role:'assistant',stopReason:v.reason??'error',errorMessage:v.error,provider:v.provider??'',usage:{input:v.input??0,cacheRead:v.cache??0,output:v.output??0}};
 return `${i}:${+isContextOverflow(message,v.capacity??0)}:${+isRecoverableLength(message,v.desired??0)}`;
});
const actual=execFileSync(native,[vectors,'--trace'],{encoding:'utf8'}).trim().split('\n');
assert.deepEqual(actual,expected);
console.log(`identical ${expected.length} overflow/recoverable-length vectors using original public source`);
