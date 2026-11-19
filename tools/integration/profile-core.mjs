// Uses original Agent + pi-ai public entrypoint from prepare_pi_diff --full-runtime.
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {createInterface} from 'node:readline';
const [directory,...raw]=process.argv.slice(2);
const [instances,turns,chunks,bytes]=raw.map(Number);
if(!directory || raw.length!==4 || raw.some(n=>!Number.isSafeInteger(+n)||+n<1))process.exit(2);
const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
const {createAssistantMessageEventStream}=await import(pathToFileURL(resolve(directory,'ai/src/index.ts')));
const text='x'.repeat(chunks*bytes);
const usage={input:0,output:0,cacheRead:0,cacheWrite:0,totalTokens:0,cost:{input:0,output:0,cacheRead:0,cacheWrite:0,total:0}};
let events=0;
const agents=Array.from({length:instances},()=>{
 const agent=new Agent({initialState:{model:{id:'profile',provider:'profile',api:'profile'}},streamFn:()=>{
  const stream=createAssistantMessageEventStream();
  const message=(value,stopReason='pending')=>({role:'assistant',content:[{type:'text',text:value}],stopReason,model:'profile',provider:'profile',api:'profile',usage,timestamp:0});
  stream.push({type:'start',partial:message('')});
  for(let i=1;i<chunks;++i)stream.push({type:'text_delta',contentIndex:0,delta:text.slice((i-1)*bytes,i*bytes),partial:message(text.slice(0,i*bytes))});
  stream.push({type:'done',reason:'stop',message:message(text,'stop')});return stream;
 }});
 agent.subscribe(()=>{++events;});return agent;
});
console.log(JSON.stringify({ready:true}));
const input=createInterface({input:process.stdin});
const lines=input[Symbol.asyncIterator]();
await lines.next();
for(let turn=0;turn<turns;++turn)for(const agent of agents)await agent.prompt('profile');
let messages=0,totalBytes=0;
for(const agent of agents){messages+=agent.state.messages.length;for(const message of agent.state.messages)for(const block of message.content)totalBytes+=Buffer.byteLength(block.text);}
console.log(JSON.stringify({messages,bytes:totalBytes,events}));

await lines.next();input.close();process.stdin.pause();
