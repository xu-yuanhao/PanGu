// Run the unmodified fixed Pi validation.ts against the actual native service.
// No production or reference edits, no npm/network: caller supplies TypeBox 1.3.27.
// node --experimental-strip-types schema-source.mjs BINARY WORKDIR NODE_MODULES
import { readFile, writeFile, mkdir, symlink, realpath } from 'node:fs/promises';
import { resolve, join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { spawnSync } from 'node:child_process';
import assert from 'node:assert/strict';
const [binary, work, modules] = process.argv.slice(2).map(p=>resolve(p));
const root=resolve(import.meta.dirname,'../..');
const source=join(root,'references/pi/packages/ai/src/utils/validation.ts');
await mkdir(work,{recursive:true});
await writeFile(join(work,'validation.ts'),await readFile(source));
try { await symlink(modules,join(work,'node_modules'),'dir'); } catch(e) { if(e.code!=='EEXIST') throw e; assert.equal(await realpath(join(work,'node_modules')),await realpath(modules),'source dependency path changed: use a fresh work directory'); }
const version=JSON.parse(await readFile(join(modules,'typebox/package.json'),'utf8')).version;
assert.equal(version,'1.3.27');
const {validateToolArguments}=await import(pathToFileURL(join(work,'validation.ts')));
const {Type}=await import(pathToFileURL(join(modules,'typebox/build/index.mjs')));
let cases=0;
function compare(schema,arguments_, label) {
 const tool={name:'echo',description:'Echo tool',parameters:schema};
 const original=structuredClone(arguments_);
 let value, passed=true;
 try { value=validateToolArguments(tool,{type:'toolCall',id:'tool-1',name:'echo',arguments:arguments_}); }
 catch { passed=false; }
 const result=spawnSync(binary,['--normalize',JSON.stringify(schema),JSON.stringify(arguments_)],{encoding:'utf8'});
 assert.equal(result.status,passed?0:2,label+': '+result.stdout+result.stderr);
 if(passed) assert.deepEqual(JSON.parse(result.stdout),value,label);
 else assert.match(result.stdout,/Validation failed for tool "echo":\n  - .+: /);
 assert.deepEqual(arguments_,original,'Pi original remains unchanged');
 ++cases;
}
const wrap=s=>({type:'object',properties:{value:s},required:['value']});
const passing=[['number','42'],['number',true],['number',null],['integer','42'],['boolean','true'],['boolean','false'],['boolean',1],['boolean',0],['string',null],['string',true],['null',''],['null',0],['null',false],[['number','string'],'1'],[['boolean','number'],'1']];
for(const [type,v] of passing) compare(wrap({type}),{value:v},'source primitive '+JSON.stringify([type,v]));
for(const [type,v] of [['boolean','1'],['boolean','0'],['null','null'],['integer','42.1']]) compare(wrap({type}),{value:v},'source rejection '+JSON.stringify([type,v]));
compare(Type.Object({count:Type.Number()}),{count:'42'},'source typed Value.Convert number');
compare(Type.Object({path:Type.String(),offset:Type.Optional(Type.Number()),nullable:Type.Optional(Type.Union([Type.String(),Type.Null()])),metadata:Type.Object({enabled:Type.Optional(Type.Boolean())})}),{path:'file.txt',offset:null,nullable:null,metadata:{enabled:null}},'source optional null');
compare({type:'object',properties:{value:{$ref:'#/$defs/value'}},$defs:{value:{anyOf:[{type:'number'},{type:'null'}]}}},{value:null},'source optional nullable ref');
compare(Type.Object({value:Type.Union([Type.Number(),Type.Null()])}),{value:null},'source typed nullable union');
compare(wrap({oneOf:[{type:'number'},{type:'null'}]}),{value:null},'source oneOf nullable');
compare(wrap({anyOf:[{type:'number'},{type:'null'}]}),{value:'42'},'source union conversion');
compare(wrap({type:['array','null'],items:{type:'string'}}),{value:null},'source nullable array');
compare({type:'array',items:[{type:'number'},{type:'boolean'}]},['42','false',7],'tuple');
compare({type:'array',items:{type:'object',properties:{x:{type:'integer'},y:{type:'boolean'}},required:['x']}},[{x:'42',y:null},{x:true}],'recursive arrays');
compare({type:'object',additionalProperties:{type:'number'}},{x:'42',y:true},'additionalProperties');
compare({allOf:[{type:'object',properties:{x:{type:'number'}}},{properties:{x:{minimum:2}}}]},{x:'3'},'allOf');
compare(wrap({oneOf:[{type:'integer'},{type:'number'}]}),{value:'42'},'ambiguous oneOf failure');
compare({type:'object',properties:{a:{anyOf:[{type:'object',properties:{x:{type:'number'}},required:['missing']},{type:'object',properties:{x:{type:'string'}}}]}}},{a:{x:'42'}},'union branch isolation');
for(const v of ['01','+42','-01','0x10','0b11','0o17','.5','1.',' 42 ', '+0x10','-0x10','0b12',' ', '', 'Infinity']) compare(wrap({type:'number'}),{value:v},'JS Number '+v);
for(const v of [1.0, -0, 1e21, 1e-7, 1e-6, 1e20, 1.2e-6]) compare(wrap({type:'string'}),{value:v},'JS String '+v);
console.log(`Pi d5b3de22 validation.ts / TypeBox ${version}: ${cases} native differential cases passed`);
