// Reuses the prepared, unmodified Pi reference modules from build/pi-diff.
// Usage: node tests/core/loop-diff.mjs build/core/pangu_core_loop_test build/pi-diff
import {execFileSync} from 'node:child_process';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import assert from 'node:assert/strict';
const directory=resolve(process.argv[3]);
const {runAgentLoop}=await import(pathToFileURL(resolve(directory,'agent/agent-loop.ts')));
const {createAssistantMessageEventStream}=await import(pathToFileURL(resolve(directory,'utils/event-stream.ts')));
const usage={input:0,output:0,cacheRead:0,cacheWrite:0,totalTokens:0,cost:{input:0,output:0,cacheRead:0,cacheWrite:0,total:0}};
let requests=0,effects=0;
function assistant(content,stopReason='pending') {
 return {role:'assistant',content,stopReason,api:'mock',provider:'mock',model:'mock',timestamp:0,usage:{...usage,cacheWrite1h:0,reasoning:0,cost:{...usage.cost,totalFromProvider:false}}};
}
const tool={name:'echo',label:'echo',description:'',parameters:{},
 async execute(_id,_args,_signal,onUpdate){++effects;const result={content:[{type:'text',text:'ok'}],details:{}};onUpdate(result);return result;}};
// Product observers publish after persistence, separately from Core's mutable
// execution context. This mode is invoked by the real HTTP/storage fixture.
if(process.argv[2]==='--published-state') {
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const published=[];let calls=0;
 const tool={name:'write',label:'write',description:'',parameters:{},async execute(){return {content:[{type:'text',text:'Successfully wrote file'}],details:{}};}};
 const agent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'}},convertToLlm:m=>m,streamFn:()=>{
  const stream=createAssistantMessageEventStream();
  stream.push({type:'start',partial:assistant([])});
  const content=calls++?[{type:'text',text:'written'}]:[{type:'toolCall',id:'write-1',name:'write',arguments:{path:'result.txt',content:'saved through plugins'}}];
  const reason=calls===1?'toolUse':'stop';stream.push({type:'done',reason,message:assistant(content,reason)});return stream;
 }});
 agent.state.tools=[{...tool,name:'read'},tool];
 const roles={system:0,user:1,assistant:2,toolResult:3};
 agent.subscribe(e=>{if(e.type==='message_start' || e.type==='message_end')published.push([e.type==='message_start'?4:6,roles[e.message.role],agent.state.messages.length]);});
 await agent.prompt({role:'user',content:[{type:'text',text:'write'}],timestamp:0});
 assert.deepEqual(JSON.parse(process.argv[4]),published);
 console.log('identical ten published message-state boundaries for product persistence');
 process.exit(0);
}
const events=[];
const messages=await runAgentLoop([{role:'user',content:[{type:'text',text:'hello'}],timestamp:0}],{messages:[],tools:[tool]},
 {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m,toolExecution:'sequential',beforeToolCall:()=>undefined,afterToolCall:()=>undefined},
 event=>events.push(event.type),undefined,()=>{
  const stream=createAssistantMessageEventStream();
  stream.push({type:'start',partial:assistant([{type:'text',text:'thinking...'}])});
  const content=++requests===1?[{type:'toolCall',id:'call-1',name:'echo',arguments:{}}]:[{type:'text',text:'done'}];
  const message=assistant(content,requests===1?'toolUse':'stop');
  stream.push({type:'done',reason:message.stopReason,message});return stream;
 });
const native=JSON.parse(execFileSync(resolve(process.argv[2]),['--trace-normal'],{encoding:'utf8'}));
assert.deepEqual(native,events);assert.equal(requests,2);assert.equal(effects,1);
function normalize(message) {
 const result={role:message.role};
 const content=typeof message.content==='string'?(message.content?[{type:'text',text:message.content}]:[]):message.content;
 result.content=content??[];
 if(message.role==='system') {
  result.toolsAdded=message.toolsAdded??[];result.toolsRemoved=message.toolsRemoved??[];
 }
 if(message.role==='assistant') for(const key of ['model','provider','api','stopReason','thinkingLevel','usage','errorMessage','providerThinkingLevel'])result[key]=message[key];
 if(message.role==='toolResult') {
  result.toolCallId=message.toolCallId;result.toolName=message.toolName;result.isError=message.isError;
  result.details=message.details??{};result.usage=message.usage??usage;
 }
 return result;
}
const body=JSON.parse(execFileSync(resolve(process.argv[2]),['--messages-normal'],{encoding:'utf8'}));
assert.deepEqual(body.context.messages.map(normalize),messages.map(normalize));
console.log(`identical ${events.length} lifecycle events, ${messages.length} normalized messages, two model requests and one tool effect`);

// Native callbacks report owned exception text before returning a status.
// Compare the actual failure artifact with unmodified Agent exception handling.
{
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const diagnostics=[];
 for(let stage=0;stage<6;++stage)for(let cancel=0;cancel<2;++cancel){
  let agent,requests=0;
  const fail=()=>{if(cancel)agent.abort();throw new Error('provider exploded');};
  agent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'}},
   prepareRequest:stage===1?fail:undefined,
   prepareNextTurn:stage===2?fail:undefined,
   transformContext:stage===3?fail:undefined,
   convertToLlm:stage===4?fail:m=>m,
   getApiKey:stage===5?fail:undefined,
   finishTurn:stage===2?()=>({action:'continue'}):undefined,
   streamFn:()=>{
    ++requests;if(stage===0)fail();
    const stream=createAssistantMessageEventStream();stream.push({type:'done',reason:'stop',message:assistant([],'stop')});return stream;
   }});
  await agent.prompt('failure');const reply=agent.state.messages.at(-1);
  assert.equal(reply.errorMessage,'provider exploded');assert.equal(agent.state.errorMessage,'provider exploded');
  assert.equal(reply.stopReason,cancel?'aborted':'error');assert.deepEqual(reply.content,[{type:'text',text:''}]);
  diagnostics.push(`diagnostic:${stage}:${cancel}:${requests}:${reply.errorMessage}`);
 }
 assert.deepEqual(execFileSync(resolve(process.argv[2]),['--trace-diagnostics'],{encoding:'utf8'}).trim().split('\n'),diagnostics);
 console.log('identical exception diagnostics for model and five preparation stages, including cancellation');
}

// Event-preserving provider: compare the metadata consumed by UI subscribers.
if(process.argv[4]) {
 const updates=[];
 const tags=['start','text_start','text_delta','text_end','thinking_start','thinking_delta','thinking_end','toolcall_start','toolcall_delta','toolcall_end','done','error'];
 await runAgentLoop([{role:'user',content:[],timestamp:0}],{messages:[],tools:[]},
  {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m,finishTurn:()=>({action:'end'})},
  event=>{if(event.type==='message_update') {
   const a=event.assistantMessageEvent;
   const value={kind:tags.indexOf(a.type),index:a.contentIndex,delta:a.delta??'',content:a.content??''};
   if(a.toolCall)value.arguments=JSON.stringify(a.toolCall.arguments);
   updates.push(value);
  }},undefined,()=>{
   const stream=createAssistantMessageEventStream();
   const partial=assistant([]);
   const emit=(type,index,extra={})=>stream.push({type,contentIndex:index,partial,...extra});
   stream.push({type:'start',partial});
   partial.content.push({type:'text',text:''});emit('text_start',0);
   partial.content[0].text+='hello ';emit('text_delta',0,{delta:'hello '});
   partial.content[0].text+='世界';emit('text_delta',0,{delta:'世界'});
   emit('text_end',0,{content:'hello 世界'});
   partial.content.push({type:'thinking',thinking:''});emit('thinking_start',1);
   partial.content[1].thinking='reason';emit('thinking_delta',1,{delta:'reason'});
   emit('thinking_end',1,{content:'reason'});
   partial.content.push({type:'toolCall',id:'one',name:'lookup',arguments:{}});emit('toolcall_start',2);
   emit('toolcall_delta',2,{delta:'{"value":'});emit('toolcall_delta',2,{delta:'1}'});
   partial.content[2].arguments={value:1};emit('toolcall_end',2,{toolCall:partial.content[2]});
   const message={...partial,stopReason:'toolUse'};stream.push({type:'done',reason:'toolUse',message});return stream;
  });
 const nativeUpdates=JSON.parse(execFileSync(resolve(process.argv[4]),['--trace-events'],{encoding:'utf8'}));
 assert.deepEqual(nativeUpdates,updates);
 console.log(`identical ${updates.length} assistant event tags, indices, deltas, end content and final tool arguments`);
}

// Stateful Agent observations must match at callback boundaries, not only idle.
if(process.argv[5]) {
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const observed=[];
 const agent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'}},streamFn:()=>createAssistantMessageEventStream()});
 const queue=()=>observed.push('queue:'+agent.peekQueuedMessages().map(m=>m.content[0].text).join(''));
 const user=text=>({role:'user',content:[{type:'text',text}],timestamp:0});
 queue();agent.steer(user('1'));agent.steer(user('2'));agent.followUp(user('f'));agent.followUp(user('g'));
 queue();queue();agent.steeringMode='all';queue();agent.followUpMode='all';agent.clearSteeringQueue();queue();
 agent.followUpMode='one-at-a-time';queue();agent.reset();queue();
 agent.steer(user('1'));agent.steer(user('1'));agent.followUp(user('f'));queue();
 agent.clearFollowUpQueue();queue();agent.steeringMode='one-at-a-time';queue();
 agent.steeringMode='all';queue();agent.clearSteeringQueue();queue();
 agent.followUp(user('f'));agent.followUp(user('g'));agent.followUpMode='all';queue();
 agent.reset();queue();assert.equal(agent.followUpMode,'all');assert.equal(agent.steeringMode,'all');
 let mode=0;
 const record=phase=>observed.push(`${mode}:${phase}:${agent.state.errorMessage?1:0}:${agent.state.streamingMessage?1:0}:${agent.state.isStreaming?1:0}`);
 agent.finishTurn=()=>{record('finish');};
 agent.subscribe(e=>{
  if(e.type==='message_start' && e.message.role==='assistant')record('start');
  if(e.type==='message_update')record('update');
  if(e.type==='message_end' && e.message.role==='assistant')record('end');
  if(e.type==='turn_end')record('turn');
  if(e.type==='agent_end')record('agent');
 });
 agent.streamFunction=()=>{
  const stream=createAssistantMessageEventStream();
  const message=assistant([{type:'text',text:'answer'}],mode===1?'error':mode===2?'aborted':'stop');
  if(mode<3)message.errorMessage='diagnostic';
  stream.push({type:'start',partial:message});
  stream.push({type:'text_delta',contentIndex:0,delta:'answer',partial:message});
  stream.push(mode===1 || mode===2?{type:'error',reason:message.stopReason,error:message}:{type:'done',reason:'stop',message});
  return stream;
 };
 for(mode=0;mode<4;++mode){await agent.prompt(user('prompt'));record('idle');}
 const nativeState=execFileSync(resolve(process.argv[5]),['--trace-state'],{encoding:'utf8'}).trim().split('\n');
 assert.deepEqual(nativeState,observed);
 console.log(`identical ${observed.length} queue previews and state observations across stop/error/aborted/retry`);
 let cancelledAgent,releaseStart,cancelCalls=0,queued=false;
 const inputs=['p','s1','s2','f1','f2'];
 cancelledAgent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'}},
  finishTurn:()=>({action:'end'}),streamFn:(_model,context,options)=>{
   assert.equal(context.messages.at(-1).content[0].text,inputs[cancelCalls]);
   const stream=createAssistantMessageEventStream();
   if(cancelCalls++===0){
    const started=new Promise(resolve=>{releaseStart=resolve;});
    queueMicrotask(async()=>{const message=assistant([{type:'text',text:'partial'}]);
     stream.push({type:'start',partial:message});await started;
     assert.equal(options.signal.aborted,true);message.stopReason='aborted';message.errorMessage='Operation aborted';
     stream.push({type:'error',reason:'aborted',error:message});});
   }else{const message=assistant([{type:'text',text:'answer'}],'stop');stream.push({type:'done',reason:'stop',message});}
   return stream;
  }});
 cancelledAgent.subscribe(e=>{
  if(e.type!=='message_start'||e.message.role!=='assistant'||queued)return;
  queued=true;cancelledAgent.steer(user('s1'));cancelledAgent.steer(user('s2'));
  cancelledAgent.followUp(user('f1'));cancelledAgent.followUp(user('f2'));
  cancelledAgent.abort();releaseStart();
 });
 const resumes=[];
 for(let step=0;step<5;++step){
  if(step)await cancelledAgent.continue();else await cancelledAgent.prompt(user('p'));
  const state=cancelledAgent.state,last=state.messages.at(-1);
  assert.equal(state.isStreaming,false);assert.equal(last.content[0].text,step?'answer':'partial');
  resumes.push(`resume:${step}:${cancelledAgent.peekQueuedMessages()[0]?.content[0].text??''}:${state.messages.length}:${last.stopReason==='aborted'?5:1}:${state.errorMessage?1:0}`);
 }
 await assert.rejects(cancelledAgent.continue(),/Cannot continue from message role: assistant/);
 assert.equal(cancelCalls,5);
 assert.deepEqual(execFileSync(resolve(process.argv[5]),['--trace-cancel-queues'],{encoding:'utf8'}).trim().split('\n'),resumes);
 console.log('identical queues retained on callback cancellation, steering/follow-up resume order and cleared error state');
 // agent.test.ts:473/538: settled callbacks must not publish progress, including
 // while another parallel operation keeps the run alive. Native callbacks are
 // retained only through release, so the C fixture exercises that live window.
 const lateTraces=[];
 for(const parallel of [0,1]) {
  let delayedUpdate,releaseSlow,slowStarted=false,updates=0,ends=0,events=0,ignored=false;
  const result={content:[{type:'text',text:'done'}],details:{},terminate:true};
  const settled={name:'tool',label:'tool',description:'',parameters:{},
   async execute(_id,_args,_signal,onUpdate){
    delayedUpdate=onUpdate;
    if(!parallel)onUpdate({content:[{type:'text',text:'running'}],details:{}});
    return result;
   }};
  const slow={...settled,name:'slow',async execute(){
   slowStarted=true;await new Promise(resolve=>{releaseSlow=resolve;});return result;
  }};
  const lateAgent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'},tools:parallel?[settled,slow]:[settled]},
   streamFn:()=>{
    const stream=createAssistantMessageEventStream();
    const calls=[{type:'toolCall',id:'a',name:'tool',arguments:{}}];
    if(parallel)calls.push({type:'toolCall',id:'b',name:'slow',arguments:{}});
    stream.push({type:'done',reason:'toolUse',message:assistant(calls,'toolUse')});return stream;
   }});
  lateAgent.subscribe(event=>{
   ++events;
   if(event.type==='tool_execution_update')++updates;
   if(event.type==='tool_execution_end') {
    ++ends;
    if(event.toolCallId==='a') {
     if(parallel)assert.equal(slowStarted,true);
     const before=events;
     delayedUpdate({content:[{type:'text',text:'late'}],details:{}});
     ignored=events===before;
     if(parallel)releaseSlow();
    }
   }
  });
  await lateAgent.prompt('run tools');
  assert.equal(ignored,true);
  lateTraces.push(`late:${parallel}:${updates}:${ends}:${+ignored}`);
 }
 assert.deepEqual(execFileSync(resolve(process.argv[5]),['--trace-late-updates'],{encoding:'utf8'}).trim().split('\n'),lateTraces);
 console.log('identical settled-tool update suppression in single and live parallel runs');
 const continuationTraces=[];
 async function continuationCase(name,tail,queues,sm,fm,mode=0) {
  let requests=0,prepares=0,next=0,agent;
  const histories=[];
  const messages=[];
  if(tail!==3&&tail!==4)messages.push(user('u'));
  if(tail===0||tail===2)messages.push(assistant(tail===2?[{type:'toolCall',id:'call-1',name:'noop',arguments:{}}]:[],tail===2?'toolUse':'stop'));
  if(tail===2)messages.push({role:'toolResult',toolCallId:'call-1',toolName:'noop',content:[],isError:false,timestamp:3});
  if(tail===4||tail===6)messages.push({role:'system',content:'',timestamp:0});
  if(tail===5)messages.push({role:'note',content:[],timestamp:0});
  agent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'},messages},convertToLlm:m=>m,
   prepareRequest:()=>{
    ++prepares;
    if(mode===1&&prepares===1)agent.steer(user('s2'));
    if(mode===3){agent.abort();throw new Error('cancelled preparation');}
   },prepareNextTurn:()=>{++next;if(mode===2&&next===1)agent.steer(user('s2'));},
   finishTurn:()=>mode===6?{action:'end'}:mode===4||mode===5?{action:'continue'}:undefined,
   streamFn:(_model,context)=>{
    ++requests;histories.push(context.messages.filter(m=>m.role==='user').map(m=>m.content[0].text).join(','));
    const stream=createAssistantMessageEventStream(),reason=mode===4?'error':mode===5?'aborted':'stop';
    const message=assistant([{type:'text',text:'done'}],reason);
    if(reason!=='stop'){message.errorMessage=reason;stream.push({type:'error',reason,error:message});}
    else stream.push({type:'done',reason,message});
    return stream;
   }});
  agent.steeringMode=sm?'all':'one-at-a-time';agent.followUpMode=fm?'all':'one-at-a-time';
  if(queues&1){agent.steer(user('s1'));agent.steer(user('s2'));}
  if(queues&2){agent.followUp(user('f1'));agent.followUp(user('f2'));}
  agent.subscribe(e=>{if(mode>=4&&e.type==='message_end'&&e.message.role==='assistant')agent.steer(user('s2'));});
  let status='ok';
  try{await agent.continue();}catch(error){assert.match(error.message,/No messages to continue|Cannot continue from message role/);status='invalid';}
  if(status!=='invalid'){
   const reason=agent.state.messages.at(-1)?.stopReason;
   if(reason==='error')status='error';else if(reason==='aborted')status='aborted';
  }
  // Inspect both retained queues through public selection/clear operations only.
  agent.steeringMode='all';agent.followUpMode='all';
  const leftS=agent.peekQueuedMessages().filter(m=>m.content[0].text.startsWith('s')).length;
  agent.clearSteeringQueue();const leftF=agent.peekQueuedMessages().length;
  continuationTraces.push(`continuation:${name}:${status}:${requests}:${histories.join('/')||'-'}:${leftS}:${leftF}:${prepares},${next}`);
 }
 for(let tail=0;tail<3;++tail)for(let q=0;q<3;++q)for(let sm=0;sm<2;++sm)for(let fm=0;fm<2;++fm)
  await continuationCase(`${['assistant','user','tool'][tail]}-${['steering','follow','both'][q]}-${['one','all'][sm]}-${['one','all'][fm]}`,tail,q+1,sm,fm);
 for(const [name,tail,queues,mode] of [
  ['assistant-empty',0,0,0],['empty-queued',3,3,0],['system-queued',4,3,0],['user-empty',1,0,0],['tool-empty',2,0,0],
  ['custom-both',5,3,0],['system-tail',6,0,0],['request-queue',1,0,1],['next-queue',0,2,2],['request-cancel',0,3,3],
  ['error-continue',1,2,4],['aborted-continue',1,2,5],['finish-end',1,2,6]])await continuationCase(name,tail,queues,0,0,mode);
 assert.deepEqual(execFileSync(resolve(process.argv[5]),['--trace-continuation'],{encoding:'utf8'}).trim().split('\n'),continuationTraces);
 console.log('identical 49 continuation startup, queue mode, preparation and hard-exit traces');
 let busyChecked=0;
 const busyAgent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'}},streamFn:()=>{
  const stream=createAssistantMessageEventStream();
  stream.push({type:'start',partial:assistant([{type:'text',text:''}])});
  stream.push({type:'done',reason:'stop',message:assistant([{type:'text',text:'Done'}],'stop')});return stream;
 }});
 busyAgent.subscribe(async e=>{
  if(e.type!=='message_start'||e.message.role!=='assistant')return;
  ++busyChecked;const original=busyAgent.state.messages[0];
  assert.equal(busyAgent.state.isStreaming,true);assert.ok(busyAgent.state.streamingMessage);
  assert.equal(busyAgent.state.messages.length,1);
  assert.throws(()=>busyAgent.reset(),/already processing/);
  await assert.rejects(busyAgent.prompt('nested'),/already processing/);
  await assert.rejects(busyAgent.continue(),/already processing/);
  assert.equal(busyAgent.state.messages.length,1);assert.equal(busyAgent.state.messages[0],original);
  assert.equal(busyAgent.state.isStreaming,true);
 });
 await busyAgent.prompt('Hello');
 assert.deepEqual(busyAgent.state.messages.map(m=>m.role),['user','assistant']);
 assert.equal(execFileSync(resolve(process.argv[5]),['--trace-streaming-busy'],{encoding:'utf8'}).trim(),
  `busy:${busyChecked}:${busyAgent.state.messages.length}:${+busyAgent.state.isStreaming}`);
 console.log('identical reset/prompt/continue rejection during an uncommitted assistant snapshot');
 const hookTraces=[];
 for(const mode of [0,1]) {
  let agent,requests=0,next=0,finishes=0,signals=0,roles='';
  const record=(signal,messages)=>{
   assert.ok(signal instanceof AbortSignal);assert.equal(signal.aborted,false);assert.equal(agent.state.isStreaming,true);++signals;
   roles=messages.map(m=>({system:0,user:1,assistant:2,toolResult:3})[m.role]).join('');assert.equal(roles,'0123');
  };
  const tool={name:'noop',label:'Noop',description:'',parameters:{},async execute(){return {content:[{type:'text',text:'tool complete'}],details:{}};}};
  agent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'},tools:[tool]},
   prepareNextTurn:mode?undefined:signal=>{++next;record(signal,agent.state.messages);},
   finishTurn:mode?(context,signal)=>{++finishes;record(signal,context.context.messages);assert.equal(context.toolResults.length,1);return {action:'end'};}:undefined,
   streamFn:()=>{
    const stream=createAssistantMessageEventStream();++requests;
    const content=requests===1?[{type:'toolCall',id:'tool-1',name:'noop',arguments:{}}]:[{type:'text',text:'done'}];
    const reason=requests===1?'toolUse':'stop';stream.push({type:'done',reason,message:assistant(content,reason)});return stream;
   }});
  await agent.prompt('start');hookTraces.push(`hooks:${mode}:${requests}:${next}:${finishes}:${signals}:${roles}`);
 }
 assert.deepEqual(execFileSync(resolve(process.argv[5]),['--trace-legacy-hooks'],{encoding:'utf8'}).trim().split('\n'),hookTraces);
 console.log('identical active-run legacy next-turn and contextual finish hooks after tool commit');
 // Awaited subscriber settlement is a synchronous callback-return barrier in
 // C. Compare order/cancellation/reuse, and also exercise the JS promises while
 // its first listener is suspended, without substituting a fixed sleep.
 let stage=0,rounds=0,cancelled=0,entered,release;
 const settlementAgent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'}},streamFn:()=>{
  const stream=createAssistantMessageEventStream();stream.push({type:'done',reason:'stop',message:assistant([],'stop')});return stream;
 }});
 settlementAgent.subscribe(e=>{if(e.type==='agent_end'){assert.equal(stage,0);stage=1;}});
 settlementAgent.subscribe(async(e,signal)=>{
  if(e.type!=='agent_end')return;
  assert.equal(stage,1);stage=2;assert.equal(signal.aborted,false);
  assert.equal(settlementAgent.state.isStreaming,true);
  entered();await new Promise(resolve=>{release=resolve;});
  settlementAgent.abort();++cancelled;
 });
 settlementAgent.subscribe((e,signal)=>{
  if(e.type==='agent_end'){assert.equal(stage,2);assert.equal(signal.aborted,true);stage=3;++rounds;}
 });
 const settlementTraces=[];
 for(let round=0;round<2;++round){
  stage=0;let promptResolved=false,idleResolved=false;
  const listenerEntered=new Promise(resolve=>{entered=resolve;});
  const prompt=settlementAgent.prompt('hello').then(()=>{promptResolved=true;});
  const idle=settlementAgent.waitForIdle().then(()=>{idleResolved=true;});
  await listenerEntered;await Promise.resolve();
  assert.equal(promptResolved,false);assert.equal(idleResolved,false);assert.equal(stage,2);
  release();await Promise.all([prompt,idle]);
  assert.equal(promptResolved,true);assert.equal(idleResolved,true);
  settlementTraces.push(`barrier:${round}:${stage}:${rounds}:${cancelled}:${+settlementAgent.state.isStreaming}:${+idleResolved}`);
  settlementAgent.reset();
 }
 assert.deepEqual(execFileSync(resolve(process.argv[5]),['--trace-settlement'],{encoding:'utf8'}).trim().split('\n'),settlementTraces);
 console.log('identical subscriber settlement order, active cancellation and two-run reuse');
 let persistentRequests=0,persistentEffects=0;
 let persistentTool='echo';
 const persistentAgent=new Agent({streamFn:(model,_context,options)=>{
  assert.equal(model.id,'owned');assert.equal(options.reasoning,'high');++persistentRequests;
  const content=persistentRequests%2?[{type:'toolCall',id:'one',name:persistentTool,arguments:{}}]:[];
  const reason=content.length?'toolUse':'stop',stream=createAssistantMessageEventStream();
  stream.push({type:'done',reason,message:assistant(content,reason)});return stream;
 }});
 assert.equal(persistentAgent.state.model.id,'unknown');assert.equal(persistentAgent.state.thinkingLevel,'off');
 persistentAgent.state.model={id:'owned',api:'fixture',provider:'script'};persistentAgent.state.thinkingLevel='high';
 persistentAgent.state.tools=[{name:'echo',label:'echo',description:'desc',parameters:{},constrainedSampling:true,
  async execute(){++persistentEffects;return {content:[],details:{}};}},
  {name:'second',label:'second',description:'',parameters:{},async execute(){throw new Error('unexpected second');}}];
 persistentAgent.state.messages=[{role:'seed',content:[],timestamp:0}];persistentAgent.state.messages=[];
 const persistentTraces=[];
 for(let round=0;round<2;++round){
  await persistentAgent.prompt({role:'user',content:[],timestamp:0});persistentAgent.reset();
  persistentTraces.push(`persistent:${round}:${persistentAgent.state.model.id}:${['off','minimal','low','medium','high','xhigh','max'].indexOf(persistentAgent.state.thinkingLevel)}:${persistentAgent.state.tools.length}:${persistentRequests}:${persistentEffects}`);
 }
 persistentTool='switched';persistentAgent.state.tools=[{...persistentAgent.state.tools[0],name:persistentTool}];
 await persistentAgent.prompt({role:'user',content:[],timestamp:0});persistentAgent.reset();
 persistentTraces.push(`persistent:2:${persistentAgent.state.model.id}:4:${persistentAgent.state.tools.length}:${persistentRequests}:${persistentEffects}`);
 assert.deepEqual(execFileSync(resolve(process.argv[5]),['--trace-persistent-state'],{encoding:'utf8'}).trim().split('\n'),persistentTraces);
 console.log('identical persistent model/thinking/executable loadout across two runs and reset');
}

if(process.argv[7]) {
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const {getCurrentSystemMessage}=await import(pathToFileURL(resolve(directory,'utils/transcript.ts')));
 const pendingTraces=[];
 for(const mode of [0,1]) {
  const makeTool=name=>({name,label:name,description:name==='first'?'First':'Echo input',parameters:{},async execute(){return {content:[],details:{}};}});
  const executable=makeTool(mode?'first':'echo');let calls=0;
  const pendingAgent=new Agent({initialState:{systemPrompt:'You are helpful.',tools:mode?[executable]:[],model:{id:'mock',api:'mock',provider:'mock'}},
   streamFn:(_model,context)=>{
    ++calls;assert.equal(context.messages.filter(m=>m.role==='system').length,2);
    const stream=createAssistantMessageEventStream();
    stream.push({type:'done',reason:'stop',message:assistant([],'stop')});return stream;
   }});
  pendingAgent.state.tools=[executable];
  const pending={role:'system',content:'',sections:mode?{note:'<note>x</note>'}:{skills:'<skills>x</skills>'},timestamp:1};
  if(mode){pending.toolsAdded=[{name:'second',description:'Echo input',parameters:{}}];pending.toolsRemoved=[{name:'first'}];}
  await pendingAgent.prompt([pending,{role:'user',content:'hi',timestamp:2}]);
  assert.equal(calls,1);assert.equal(pendingAgent.state.messages.length,4);
  const actual=pendingAgent.state.messages[1],current=getCurrentSystemMessage(pendingAgent.state.messages);
  assert.deepEqual(actual.sections,pending.sections);
  pendingTraces.push(`pending:${mode}:2:${actual.toolsAdded?.length??0}:${actual.toolsRemoved?.length??0}:${Object.keys(actual.sections)[0]}:${actual.timestamp}:${current.toolsAdded[0].name}`);
 }
 assert.deepEqual(execFileSync(resolve(process.argv[7]),['--trace-pending-declarations'],{encoding:'utf8'}).trim().split('\n'),pendingTraces);
 console.log('identical pending system sections, executable tool reconciliation and timestamps');
 const loops=await import(pathToFileURL(resolve(directory,'agent/agent-loop.ts')));
 const {setDefaultStreamFn}=await import(pathToFileURL(resolve(directory,'agent/stream-fn.ts')));
 const observations=[];
 try {
  for(let entry=0;entry<5;++entry)for(let explicit=0;explicit<2;++explicit){
   let fallback=0,override=0;
   const stream=()=>{const s=createAssistantMessageEventStream();const message=assistant([],'stop');
    s.push({type:'done',reason:'stop',message});return s;};
   setDefaultStreamFn(()=>{++fallback;return stream();});
   const selected=explicit?()=>{++override;return stream();}:undefined;
   const user={role:'user',content:[{type:'text',text:'prompt'}],timestamp:0};
   const config={model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:messages=>messages};
   let result;
   if(entry===0){
    const agent=new Agent({initialState:{model:config.model},streamFn:selected});
    await agent.prompt(user);result=agent.state.messages;
   }else if(entry===1)result=await loops.agentLoop([user],{messages:[],tools:[]},config,undefined,selected).result();
   else if(entry===2)result=await loops.agentLoopContinue({messages:[user],tools:[]},config,undefined,selected).result();
   else if(entry===3)result=await loops.runAgentLoop([user],{messages:[],tools:[]},config,async()=>{},undefined,selected);
   else result=await loops.runAgentLoopContinue({messages:[user],tools:[]},config,async()=>{},undefined,selected);
   assert.equal(result.at(-1).role,'assistant');assert.equal(result.at(-1).stopReason,'stop');
   observations.push(`default:${entry}:${explicit}:${fallback}:${override}:2`);
  }
 }finally{setDefaultStreamFn(undefined);}
 assert.deepEqual(execFileSync(resolve(process.argv[7]),['--trace-default-exports'],{encoding:'utf8'}).trim().split('\n'),observations);
 console.log('identical fallback and explicit-stream precedence across Agent and all four low-level exports');
 {
  const system={role:'system',content:'Transcript prompt',toolsAdded:[],timestamp:1};let calls=0,observed;
  await loops.runAgentLoop([system,{role:'user',content:'Hello',timestamp:0}],{messages:[],tools:[]},
   {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m},async()=>{},undefined,(_model,context)=>{
    ++calls;assert.equal(context.messages[0],system);assert.deepEqual(Object.keys(context),['messages']);
    observed={contextKeys:Object.keys(context),system:context.messages[0].content,systemTimestamp:context.messages[0].timestamp,
     user:context.messages[1].content,calls};
    const stream=createAssistantMessageEventStream(),message=assistant([],'stop');stream.push({type:'done',reason:'stop',message});return stream;
   });
  assert.deepEqual(JSON.parse(execFileSync(resolve(process.argv[7]),['--trace-transcript-context'],{encoding:'utf8'})),observed);
  console.log('identical transcript-only provider context and original SYSTEM prompt provenance');
 }
 const queued=[],included=[];let preparations=0,polls=0;
 const late={role:'user',content:[{type:'text',text:'late steering'}],timestamp:0};
 const messages=await loops.runAgentLoop([{role:'user',content:[{type:'text',text:'run'}],timestamp:0}],
  {messages:[],tools:[]},{model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:messages=>messages,
   getSteeringMessages:async()=>{++polls;return queued.splice(0);},
   prepareRequest:()=>{if(++preparations===1)queued.push(late);}},async()=>{},undefined,
  (_model,context)=>{included.push(context.messages.includes(late));const stream=createAssistantMessageEventStream();
   const message=assistant([],'stop');stream.push({type:'done',reason:'stop',message});return stream;});
 assert.equal(polls,3);
 assert.deepEqual(JSON.parse(execFileSync(resolve(process.argv[7]),['--trace-late-steering'],{encoding:'utf8'})),
  {included,preparations,messages:messages.length,queued:queued.length!==0});
 console.log('identical steering queued during prepareRequest waits until the following request');
 const controls=[];
 for(let mode=0;mode<10;++mode){
  let requests=0,finishes=0,effects=0,prepared=0;const steering=[],follow=[];
  const user=text=>({role:'user',content:[{type:'text',text}],timestamp:0});
  const hasTool=mode===1||(mode>=4&&mode<=6)||mode===9;
  const tool={name:'noop',label:'noop',description:'',parameters:{},async execute(){++effects;return {content:[{type:'text',text:'done'}],details:{}};}};
  const messages=await loops.runAgentLoop([user('run')],{messages:[],tools:hasTool?[tool]:[]},
   {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:messages=>messages,
    beforeToolCall:({toolCall})=>(mode===4||mode===6)&&toolCall.id==='a'?{block:true,terminate:true,reason:'blocked'}:undefined,
    afterToolCall:()=>mode===5?{terminate:true}:undefined,
    getSteeringMessages:()=>steering.splice(0),getFollowUpMessages:()=>follow.splice(0),
    prepareNextTurn:()=>{++prepared;},
    finishTurn:turn=>{
     ++finishes;const count=requests===1&&hasTool?(mode===6?2:1):0;
     assert.equal(turn.toolResults.length,count);assert.equal(turn.newMessages.length,turn.context.messages.length);
     for(const [i,result] of turn.toolResults.entries()){
      const blocked=(mode===4||mode===6)&&i===0;
      assert.equal(result.role,'toolResult');assert.equal(result.toolName,'noop');assert.equal(result.isError,blocked);
      assert.equal(result.content[0].text,blocked?'blocked':'done');assert.ok(!result.terminate);
     }
     if(finishes===1){
      if(mode===2||mode>=7)steering.push(user('steering'));
      if(mode===3||mode>=7)follow.push(user('follow-up'));
      if(mode===9)return {action:'end'};
      if(mode<4||mode===7||mode===8)return {action:'continue'};
     }
    }},async()=>{},undefined,(_model,context)=>{
    assert.ok(requests<2);++requests;
    if(requests===2&&(mode===2||mode===3))assert.equal(context.messages.at(-1).content[0].text,mode===2?'steering':'follow-up');
    const stream=createAssistantMessageEventStream();
    const content=requests===1&&hasTool?
     [{type:'toolCall',id:'a',name:'noop',arguments:{}},...(mode===6?[{type:'toolCall',id:'b',name:'noop',arguments:{}}]:[])]:[{type:'text',text:'done'}];
    const reason=mode===7?'error':mode===8?'aborted':requests===1&&hasTool?'toolUse':'stop';
    const message=assistant(content,reason);if(mode===7||mode===8)message.errorMessage='failed';
    stream.push(reason==='error'||reason==='aborted'?{type:'error',reason,error:message}:{type:'done',reason,message});return stream;
   });
  const roles=messages.map(m=>({system:'s',user:'u',assistant:'a',toolResult:'t'})[m.role]).join('');
  const errors=messages.filter(m=>m.role==='toolResult').map(m=>m.isError?'1':'0').join('');
  controls.push(`control:${mode}:${requests}:${finishes}:${effects}:${prepared}:${roles}:${steering.length}:${follow.length}:${errors}`);
 }
 assert.deepEqual(execFileSync(resolve(process.argv[7]),['--trace-controls'],{encoding:'utf8'}).trim().split('\n'),controls);
 console.log('identical ten natural-continuation, blocked/after-hook termination, hard-exit and finish-end combinations');
 const tails=[];
 for(let custom=0;custom<2;++custom){
  let converted=0,starts=0,ends=0;
  const original=custom?{role:'custom',text:'Hook content',timestamp:17}:
   {role:'user',content:'Hook content',timestamp:17};
  const context={messages:[original],tools:[]};
  const stream=loops.agentLoopContinue(context,{model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:messages=>{
   ++converted;assert.equal(messages[0],original);
   return messages.map(m=>m.role==='custom'?{role:'user',content:m.text,timestamp:m.timestamp}:m);
  }},undefined,(_model,projected)=>{
   assert.equal(projected.messages[0].role,'user');assert.equal(projected.messages[0].content,'Hook content');
   assert.equal(projected.messages[0].timestamp,17);
   const s=createAssistantMessageEventStream(),message=assistant([{type:'text',text:'Response'}],'stop');
   s.push({type:'done',reason:'stop',message});return s;
  });
  for await(const e of stream){
   if(e.type==='message_start'||e.type==='message_end'){
    assert.equal(e.message.role,'assistant');if(e.type==='message_start')++starts;else ++ends;
   }
  }
  const generated=await stream.result();assert.equal(context.messages[0],original);
  tails.push(`tail:${custom}:${converted}:${starts}:${ends}:${generated.length}:${context.messages.length}`);
 }
 assert.deepEqual(execFileSync(resolve(process.argv[7]),['--trace-tails'],{encoding:'utf8'}).trim().split('\n'),tails);
 console.log('identical USER/custom-tail continuation projection, generated-only output and no replayed input events');
 const projections=[];
 for(let mode=0;mode<2;++mode){
  let phase=0,projected=0;
  const history=mode?['old message 1','old response 1','old message 2','old response 2'].map((text,i)=>
   i%2?assistant([{type:'text',text}],'stop'):{role:'user',content:text,timestamp:0}):
   [{role:'notification',text:'This is a notification',timestamp:0}];
  const original=history[0],context={messages:history,tools:[]};
  const transformContext=mode?messages=>{assert.equal(phase,0);assert.equal(messages.length,5);phase=1;return messages.slice(-2);}:undefined;
  const generated=await loops.runAgentLoop([{role:'user',content:mode?'new message':'Hello',timestamp:0}],context,
   {model:{id:'mock',api:'mock',provider:'mock'},transformContext,convertToLlm:messages=>{
    assert.equal(phase,mode);phase=2;assert.equal(messages.length,2);return messages.filter(m=>m.role!=='notification');
   }},async()=>{},undefined,(_model,view)=>{
    assert.equal(phase,2);phase=3;projected=view.messages.length;assert.equal(projected,mode?2:1);
    assert.equal(view.messages.at(-1).content,mode?'new message':'Hello');
    if(mode)assert.equal(view.messages[0].content[0].text,'old response 2');
    const stream=createAssistantMessageEventStream(),message=assistant([],'stop');stream.push({type:'done',reason:'stop',message});return stream;
   });
  assert.equal(context.messages[0],original);assert.equal(generated.length,2);
  projections.push(`projection:${mode}:${projected}:${history.length+generated.length}:${phase}`);
 }
 assert.deepEqual(execFileSync(resolve(process.argv[7]),['--trace-projection'],{encoding:'utf8'}).trim().split('\n'),projections);
 console.log('identical custom filtering and transform-before-convert pruning without rewriting execution history');
 {
  let prepares=0,requests=0,delivered=false;const completed=[];
  const user=text=>({role:'user',content:[{type:'text',text}],timestamp:0});
  const steering=user('steering'),canonical=user('canonical projection');
  let finalContext;
  const generated=await loops.runAgentLoop([user('prompt')],{messages:[],tools:[]},
   {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m,
    getSteeringMessages:()=>{if(delivered)return [];delivered=true;return [steering];},
    prepareRequest:({context})=>{
     ++prepares;assert.ok(completed.includes(steering));assert.ok(context.messages.includes(steering));
     finalContext={...context,messages:[canonical]};
     return {context:finalContext,model:{id:'replacement',api:'mock',provider:'mock'},thinkingLevel:'high'};
    }},e=>{if(e.type==='message_end')completed.push(e.message);},undefined,(model,context,options)=>{
     ++requests;assert.equal(model.id,'replacement');assert.equal(options.reasoning,'high');assert.deepEqual(context.messages,[canonical]);
     const stream=createAssistantMessageEventStream(),message=assistant([{type:'text',text:'done'}],'stop');
     stream.push({type:'done',reason:'stop',message});return stream;
    });
  assert.equal(generated.at(-1).thinkingLevel,'high');
  assert.deepEqual(execFileSync(resolve(process.argv[7]),['--trace-initial'],{encoding:'utf8'}).trim(),
   `initial:${prepares}:${requests}:${completed.length}:${generated.length}:${finalContext.messages.length}`);
  console.log('identical initial steering publication before context/model/thinking preparation and generated-message provenance');
 }
 {
  let requests=0,prepared=0,ends=0;const queued=[];
  const user=text=>({role:'user',content:[{type:'text',text}],timestamp:0});
  const messages=await loops.runAgentLoop([user('run')],{messages:[],tools:[]},
   {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m,
    finishTurn:()=>requests===1?{action:'continue'}:undefined,
    getSteeringMessages:()=>queued.splice(0),
    prepareNextTurn:({context})=>{++prepared;queued.push(user('queued'));return {
     context:{...context,messages:context.messages.slice()},model:{id:'next-model',api:'mock',provider:'mock'},
     thinkingLevel:'high',messages:[user('prepared')]};}},
   e=>{if(e.type==='agent_end')++ends;},undefined,(model,context,options)=>{
    if(++requests===2){assert.equal(context.messages.length,4);
     assert.deepEqual(context.messages.slice(-2).map(m=>m.content[0].text),['prepared','queued']);
     assert.equal(model.id,'next-model');assert.equal(options.reasoning,'high');}
    const stream=createAssistantMessageEventStream(),message=assistant([],'stop');
    stream.push({type:'done',reason:'stop',message});return stream;
   });
  assert.equal(queued.length,0);
  assert.equal(messages.at(-1).thinkingLevel,'high');
  assert.deepEqual(execFileSync(resolve(process.argv[7]),['--trace-next'],{encoding:'utf8'}).trim(),
   `next:${requests}:${prepared}:${ends}:prepared,queued:${messages.length}`);
  console.log('identical next-turn prepared input then steering queued inside preparation, without a spare continuation');
 }
}

if(process.argv[6]) {
 const {streamProxy}=await import(pathToFileURL(resolve(directory,'agent/proxy.ts')));
 const originalFetch=globalThis.fetch;
 const model={id:'mock',provider:'mock',api:'mock',contextWindow:4096,maxTokens:512,input:['text','image']};
 const context={messages:[{role:'user',content:[{type:'text',text:'世界'}],timestamp:17},
  {role:'assistant',content:[],model:'mock',provider:'mock',api:'mock',responseModel:'mock-revision',
   responseId:'response\u0000世界',rawStopReason:'provider-stop',stopReason:'stop',thinkingLevel:'off',timestamp:0,
   diagnostics:[{type:'fixture',timestamp:17,details:{retry:false}}],
   deferred:{provider:'fixture',api:'mock',modelId:'mock',id:'handle',pollAfterMs:0,data:null},endTurn:false,
   usage:{...usage,cost:{...usage.cost,input:Infinity,output:-Infinity,total:NaN}}},
  {role:'toolResult',toolCallId:'nested',toolName:'echo',content:[],usage,isError:false,timestamp:0,
   nestedCalls:{calls:[{id:'one',name:'read',arguments:{path:'a.txt'},status:'ok',durationMs:0}],complete:false}}]};
 const cases=[{},
  {temperature:0,samplingParams:{topP:0,topK:1},maxTokens:0,reasoning:'high',cacheRetention:'long',
   sessionId:'session\u0000世界',headers:{'x-example':'value'},metadata:{nested:[null,false,0]},
   transport:'sse',thinkingBudgets:{minimal:0,high:1024},maxRetryDelayMs:0},
  {temperature:null,samplingParams:null,maxTokens:null,reasoning:null,cacheRetention:null,sessionId:null,
   headers:null,metadata:null,transport:null,thinkingBudgets:null,maxRetryDelayMs:null},
  {temperature:0.25,authToken:'synthetic-local',apiKey:'synthetic-local',customOption:true,
   proxyUrl:'https://synthetic.invalid',signal:{local:true},onPayload:'local'}];
 try {
  for(const options of cases) {
   let captured,calls=0;
   globalThis.fetch=async (url,init)=>{
    ++calls;assert.equal(url,'https://synthetic.invalid/api/stream');assert.equal(init.method,'POST');
    assert.equal(init.headers.Authorization,'Bearer synthetic-local');
    captured=JSON.parse(init.body);
    return new Response('data: '+JSON.stringify({type:'done',reason:'stop',usage})+'\n\n');
   };
   // Pass a real AbortSignal: it is transport-only and must never enter the body.
   const stream=streamProxy(model,context,{...options,proxyUrl:'https://synthetic.invalid',
    authToken:'synthetic-local',signal:new AbortController().signal});
   const result=await stream.result();assert.equal(result.stopReason,'stop');assert.equal(calls,1);
   const native=JSON.parse(execFileSync(resolve(process.argv[6]),['--request-json',JSON.stringify(options)],{encoding:'utf8'}));
   assert.deepEqual(native,captured);
  }
  const presenceRequests=[];
  for(const present of [false,true]) {
   const blocks=[{type:'text',text:'text'},{type:'thinking',thinking:'reason'},
    {type:'toolCall',id:'one',name:'lookup',arguments:{}}];
   if(present){blocks[0].textSignature='';blocks[1].thinkingSignature='';
    Object.assign(blocks[2],{thoughtSignature:'',namespace:'',argumentsError:''});}
   const fixtureModel={id:'mock',provider:'mock',api:'fixture'};
   const fixtureContext={messages:[{role:'assistant',content:blocks,model:'mock',provider:'mock',api:'fixture',
    timestamp:23,stopReason:'toolUse',thinkingLevel:'off',usage}]};
   globalThis.fetch=async(_url,init)=>{presenceRequests.push(JSON.parse(init.body));
    return new Response('data: '+JSON.stringify({type:'done',reason:'stop',usage})+'\n\n');};
   await streamProxy(fixtureModel,fixtureContext,{proxyUrl:'https://synthetic.invalid',authToken:'synthetic-local'}).result();
  }
  assert.deepEqual(execFileSync(resolve(process.argv[6]),['--trace-content-presence'],{encoding:'utf8'}).trim().split('\n').map(JSON.parse),presenceRequests);
  console.log('identical absent versus explicitly empty optional content strings after owned transcript copy');
  // Independent JS fixtures for the native complete-message wire matrix.
  const wireText='a\u0000世界\n"\\';
  const wireBlocks=[{type:'text',text:wireText,textSignature:'text-sig'},{type:'image',data:'AAAA',mimeType:'image/png'}];
  const wireSchema={type:'object',properties:{items:{type:'array',items:{anyOf:[{type:'null'},{type:'string',enum:['','世界']}]}}},required:['items'],additionalProperties:false};
  const wireContext={messages:[
   {role:'system',content:[wireBlocks[0]],sections:{empty:'',removed:null},toolsAdded:[
    {name:'lookup',description:'世界',parameters:wireSchema,constrainedSampling:false},
    {name:'other',description:'',parameters:{type:'object'},constrainedSampling:true}],toolsRemoved:[{name:'old'},{name:'世界'}],timestamp:17},
   {role:'user',content:wireBlocks,timestamp:18},
   {role:'assistant',content:[wireBlocks[0],{type:'thinking',thinking:'reason',thinkingSignature:'think-sig',redacted:true},
    {type:'toolCall',id:'one',name:'lookup',arguments:{items:[null,'','世界'],zero:0,flag:false},thoughtSignature:'thought-sig',namespace:'dynamic_tools',argumentsError:'fixture-error'}],
    model:'mock',provider:'mock',api:'fixture',timestamp:19,stopReason:'length',thinkingLevel:'max',durationMs:0,endTurn:true,errorMessage:'fixture-error',
    usage:{input:1,output:2,cacheRead:3,cacheWrite:4,totalTokens:10,cacheWrite1h:4,reasoning:2,
     cost:{input:.1,output:.2,cacheRead:.3,cacheWrite:.4,total:1,totalFromProvider:true}}},
   {role:'toolResult',toolCallId:'one',toolName:'lookup',content:wireBlocks,timestamp:20,durationMs:7,isError:false,
    details:[null,false,{value:0}],usage}]};
  const wireModel={id:'mock',provider:'mock',api:'fixture',name:'世界',baseUrl:'https://invalid.example/v1',reasoning:true,
   input:['text','image'],cost:{input:0},contextWindow:4096,maxTokens:512,compat:{nested:[false,null]}};
  const wireValues={temperature:0,samplingParams:{top_p:0,nested:[null,false,'世界']},maxTokens:0,reasoning:'off',cacheRetention:'none',
   sessionId:'s\u0000世界',headers:{'x-test':'世界'},metadata:{zero:0,false:false,empty:''},transport:'sse',thinkingBudgets:{minimal:0,high:4096},maxRetryDelayMs:0};
  const wireRequests=[];
  for(const options of [{},wireValues,Object.fromEntries(Object.keys(wireValues).map(key=>[key,null]))]){
   globalThis.fetch=async(_url,init)=>{wireRequests.push(JSON.parse(init.body));return new Response('data: '+JSON.stringify({type:'done',reason:'stop',usage})+'\n\n');};
   await streamProxy(wireModel,wireContext,{...options,apiKey:'fixture',customOption:true,onPayload:true,onResponse:true,
    signal:new AbortController().signal,proxyUrl:'https://synthetic.invalid',authToken:'synthetic-local'}).result();
  }
  assert.deepEqual(execFileSync(resolve(process.argv[6]),['--trace-wire'],{encoding:'utf8'}).trim().split('\n').map(JSON.parse),wireRequests);
  console.log('identical three complete multimodal requests, nested Schema/model metadata and all eleven options');
  const lateEvents=[{type:'start'},{type:'text_start',contentIndex:0},
   {type:'text_delta',contentIndex:0,delta:'kept'},
   ...[0,4].map(contentIndex=>({type:'toolcall_end',contentIndex,
    toolCall:{type:'toolCall',id:'late',name:'unused',arguments:{}}})),
   ...[-1,null,0].map(contentIndex=>({type:'toolcall_end',contentIndex})),{type:'toolcall_end'},
   {type:'done',reason:'stop',usage:{}}];
  globalThis.fetch=async ()=>new Response(lateEvents.map((e,i)=>i===0?
   'data: \ufeff'+JSON.stringify(e)+'\u00a0\u2028\ndata: \ufeff\u00a0\n':'data: '+JSON.stringify(e)+'\n').join(''));
  const lateStream=streamProxy(model,context,{proxyUrl:'https://synthetic.invalid',authToken:'synthetic-local'});
  let updates=0;for await(const event of lateStream)if(event.type!=='done' && event.type!=='error')++updates;
  const lateResult=await lateStream.result();
  assert.deepEqual(JSON.parse(execFileSync(resolve(process.argv[4]),['--trace-ignored-tool-end'],{encoding:'utf8'})),
   {stopReason:lateResult.stopReason,text:lateResult.content[0].text,updates});
  console.log('identical ignored toolcall_end for replaced/missing blocks, retained text and event count');
  const repeatedEnds=[];
  for(let clear=0;clear<2;++clear){
   const toolCall={type:'toolCall',id:'one',name:'lookup',arguments:{}};
   const events=[{type:'toolcall_start',contentIndex:0,id:'one',toolName:'lookup'},
    {type:'toolcall_end',contentIndex:0,toolCall:{...toolCall,namespace:'ns',thoughtSignature:'sig',argumentsError:'bad'}},
    {type:'toolcall_end',contentIndex:0,toolCall:{...toolCall,arguments:{value:2}}}];
   if(clear)events.push({type:'toolcall_end',contentIndex:0,toolCall:{...toolCall,arguments:{value:2},namespace:'',thoughtSignature:'',argumentsError:''}});
   events.push({type:'done',reason:'toolUse',usage:{}});
   globalThis.fetch=async ()=>new Response(events.map(e=>'data: '+JSON.stringify(e)+'\n').join(''));
   const stream=streamProxy(model,context,{proxyUrl:'https://synthetic.invalid',authToken:'synthetic-local'});
   let updates=0;for await(const event of stream)if(event.type!=='done'&&event.type!=='error')++updates;
   const result=await stream.result(),call=result.content[0];
   assert.deepEqual(call.arguments,{value:2});assert.equal(result.stopReason,'toolUse');
   repeatedEnds.push({namespace:call.namespace,signature:call.thoughtSignature,argumentsError:call.argumentsError,updates});
  }
  assert.deepEqual(execFileSync(resolve(process.argv[4]),['--trace-repeated-tool-end'],{encoding:'utf8'}).trim().split('\n').map(x=>JSON.parse(x)),repeatedEnds);
  console.log('identical repeated toolcall_end optional metadata retention and explicit empty clearing');
  globalThis.fetch=async ()=>new Response('data: '+JSON.stringify({type:'text_start',contentIndex:0})+'\n'+
   'data: '+JSON.stringify({type:'text_delta',contentIndex:0,delta:'kept'}));
  const eofStream=streamProxy(model,context,{proxyUrl:'https://synthetic.invalid',authToken:'synthetic-local'});
  let finals=0;for await(const event of eofStream)if(event.type==='error'||event.type==='done')++finals;
  const eofResult=await eofStream.result();
  assert.deepEqual(JSON.parse(execFileSync(resolve(process.argv[4]),['--trace-eof'],{encoding:'utf8'})),
   {stopReason:eofResult.stopReason,text:eofResult.content[0].text,error:eofResult.errorMessage,finals});
  console.log('identical EOF partial text, exact diagnostic and single final error without trailing newline');
  const cancelledEof=[];
  const beforeTimestamp=Date.now();
  globalThis.fetch=async ()=>new Response([
   {type:'start'},{type:'text_start',contentIndex:0},{type:'text_delta',contentIndex:0,delta:'kept'},
   {type:'done',reason:'stop',usage:{}}
  ].map(e=>'data: '+JSON.stringify(e)+'\n').join(''));
  const timestampStream=streamProxy(model,context,{proxyUrl:'https://synthetic.invalid',authToken:'synthetic-local'});
  const timestamps=[];
  for await(const event of timestampStream)timestamps.push((event.partial??event.message??event.error).timestamp);
  const afterTimestamp=Date.now();
  assert.deepEqual(JSON.parse(execFileSync(resolve(process.argv[4]),['--trace-timestamp'],{encoding:'utf8'})),
   {withinRequest:timestamps.every(t=>t>=beforeTimestamp&&t<=afterTimestamp),
    stable:timestamps.every(t=>t===timestamps[0]),snapshots:timestamps.length});
  console.log('identical request-time UTC timestamp retained across proxy start/delta/final snapshots');
  for(const tail of ['data: {invalid}',
   'data: '+JSON.stringify({type:'text_delta',contentIndex:0,delta:'discarded'}),
   'data: '+JSON.stringify({type:'done',reason:'stop',usage:{}})]){
   const controller=new AbortController();let reads=0;
   const bytes=new TextEncoder().encode(
    'data: '+JSON.stringify({type:'text_start',contentIndex:0})+'\n'+
    'data: '+JSON.stringify({type:'text_delta',contentIndex:0,delta:'kept'})+'\n'+tail);
   globalThis.fetch=async ()=>({ok:true,body:{getReader:()=>({
    read:async ()=>{if(reads++===0)return {done:false,value:bytes};controller.abort();return {done:true};},
    cancel:async ()=>{},releaseLock:()=>{}
   })}});
   const stream=streamProxy(model,context,{proxyUrl:'https://synthetic.invalid',authToken:'synthetic-local',signal:controller.signal});
   let finals=0;for await(const event of stream)if(event.type==='error'||event.type==='done')++finals;
   const result=await stream.result();
   cancelledEof.push({stopReason:result.stopReason,text:result.content[0].text,error:result.errorMessage,finals});
  }
  assert.deepEqual(execFileSync(resolve(process.argv[4]),['--trace-cancelled-eof'],{encoding:'utf8'}).trim().split('\n').map(x=>JSON.parse(x)),cancelledEof);
  console.log('identical abort-before-EOF-tail ordering for malformed, text and terminal buffered lines');
  const mismatch=[];
  for(const [i,type] of ['text_delta','text_end','thinking_delta','thinking_end','toolcall_delta'].entries()){
   globalThis.fetch=async ()=>new Response([
    {type:i<2?'thinking_start':'text_start',contentIndex:0},{type,contentIndex:0,delta:'bad'}
   ].map(e=>'data: '+JSON.stringify(e)+'\n').join(''));
   const result=await streamProxy(model,context,{proxyUrl:'https://synthetic.invalid',authToken:'synthetic-local'}).result();
   assert.equal(result.stopReason,'error');assert.equal(result.content.length,1);mismatch.push(result.errorMessage);
  }
  assert.deepEqual(execFileSync(resolve(process.argv[4]),['--trace-mismatch'],{encoding:'utf8'}).trim().split('\n'),mismatch);
  console.log('identical five proxy content-type mismatch diagnostics with retained partial block');
 } finally {globalThis.fetch=originalFetch;}
 console.log('identical proxy request bodies: all eleven options, absent/null/zero, Unicode/NUL and local-only fields');
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const observations=[];
 const agent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'}},sessionId:'session-abc',
  onProviderStreamEvent:(data,model)=>observations.push(`event:${data.request_cost}:${model.id}`),
  streamFn:async (model,_context,options)=>{
   observations.push(`session:${options.sessionId}`);
   await options.onProviderStreamEvent({request_cost:0.01},model);
   const stream=createAssistantMessageEventStream();
   stream.push({type:'done',reason:'stop',message:assistant([],'stop')});return stream;
  }});
 await agent.prompt('first');agent.sessionId='session-def';await agent.prompt('second');
 const nativeOptions=execFileSync(resolve(process.argv[6]),['--trace-options'],{encoding:'utf8'}).trim().split('\n');
 assert.deepEqual(nativeOptions,observations);
 console.log('identical sessionId forwarding/override and provider raw event observations');
}

if(process.argv[7]) {
 const keyTrace=[];
 for(let mode=0;mode<3;++mode) {
  let resolved=0,called=0,converted=0;
  await runAgentLoop([{role:'user',content:[],timestamp:0}],{messages:[],tools:[]},
   {model:{id:'mock',api:'mock',provider:'mock'},apiKey:'fallback',
    convertToLlm:messages=>{++converted;return messages;},
    getApiKey:provider=>{
     assert.equal(provider,'mock');assert.equal(converted,resolved+1);
     keyTrace.push(`resolve:${++resolved}`);
     return mode===0?`k${resolved}!`:mode===1?'':undefined;
    },finishTurn:()=>({action:mode===0 && called===1?'continue':'end'})},
   ()=>{},undefined,(_model,_context,options)=>{
    ++called;assert.equal(options.apiKey,mode===0?`k${called}!`:'fallback');
    keyTrace.push(`provider:${mode===0?'resolved':'fallback'}`);
    const stream=createAssistantMessageEventStream();
    stream.push({type:'done',reason:'stop',message:assistant([],'stop')});return stream;
   });
 }
 const nativeKeys=execFileSync(resolve(process.argv[7]),['--trace-keys'],{encoding:'utf8'}).trim().split('\n');
 assert.deepEqual(nativeKeys,keyTrace);
 console.log('identical per-request credential refresh order and empty/absent fallback; key values omitted');
}

if(process.argv[8]) {
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const hookTrace=[];
 for(let mode=0;mode<3;++mode) {
  const agent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'}},
   onPayload:(payload,model)=>{
    assert.equal(model.id,'mock');assert.deepEqual(payload,{original:true});hookTrace.push('payload');
    return mode===0?{changed:true}:mode===1?undefined:null;
   },
   onResponse:(response,model)=>{
    assert.equal(model.id,'mock');assert.deepEqual(response,{status:200,headers:{'x-test':'present'}});
    hookTrace.push('response:200');
   },streamFn:async (model,_context,options)=>{
    let payload={original:true};const next=await options.onPayload(payload,model);
    if(next!==undefined)payload=next;
    hookTrace.push('send:'+JSON.stringify(payload));
    await options.onResponse({status:200,headers:{'x-test':'present'}},model);hookTrace.push('body');
    const stream=createAssistantMessageEventStream();
    stream.push({type:'done',reason:'stop',message:assistant([],'stop')});return stream;
   }});
  await agent.prompt('request');
 }
 const nativeHooks=execFileSync(resolve(process.argv[8]),['--trace-hooks'],{encoding:'utf8'}).trim().split('\n');
 assert.deepEqual(nativeHooks,hookTrace);
 console.log('identical payload replace/undefined/null and response-before-body hook traces');
}

if(process.argv[9]) {
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const pending=[];
 const tool={name:'tool',label:'tool',description:'',parameters:{},async execute(){return {content:[],details:{}};}};
 const agent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'},tools:[tool]},
  finishTurn:()=>({action:'end'}),streamFn:()=>{
   const stream=createAssistantMessageEventStream();
   const call={type:'toolCall',id:'duplicate',name:'tool',arguments:{}};
   stream.push({type:'done',reason:'toolUse',message:assistant([{...call},{...call}],'toolUse')});return stream;
  }});
 agent.subscribe(event=>{
  if(event.type==='tool_execution_start')pending.push('start:'+agent.state.pendingToolCalls.size);
  if(event.type==='tool_execution_end')pending.push('end:'+agent.state.pendingToolCalls.size);
 });
 await agent.prompt('tools');
 const nativePending=execFileSync(resolve(process.argv[9]),['--trace-pending'],{encoding:'utf8'}).trim().split('\n');
 assert.deepEqual(nativePending,pending);
 console.log('identical duplicate tool-ID pending Set observations');
}

// Complete tool-side-effect and continuation scenarios from the native fixture.
for(const mode of [0,1,3,4,6,7,8,9,11,12,13]) {
 let requests=0,effects=0,finished=0;
 const controller=new AbortController();
 const events=[];
 const tool={name:'echo',label:'echo',description:'',parameters:{},
  async execute(_id,_args,_signal,onUpdate){++effects;const result={content:[{type:'text',text:'ok'}],details:{},...(mode===8?{terminate:true}:{})};onUpdate(result);return result;}};
 const messages=await runAgentLoop([{role:'user',content:[{type:'text',text:'hello'}],timestamp:0}],{messages:[],tools:[tool]},
  {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m,toolExecution:'sequential',
   beforeToolCall:()=>mode===3?{block:true,reason:'Denied'}:undefined,afterToolCall:()=>undefined,
   finishTurn:()=>{++finished;return mode===9 && requests===1?{action:'continue'}:undefined;}},
  e=>events.push(e.type),controller.signal,()=>{
   const stream=createAssistantMessageEventStream();
   ++requests;
   if(mode===6 || mode===7 || mode>=11) {
    const aborted=mode===7 || mode>=12;
    if(aborted)controller.abort();
    if(mode>=11)stream.push({type:'start',partial:assistant([{type:'text',text:'thinking...'}])});
    const message=assistant([{type:'text',text:mode>=11?'thinking...':aborted?'Operation aborted':'Model stream failed'}],aborted?'aborted':'error');
    message.errorMessage=mode===13?'provider aborted':aborted?'Operation aborted':'Model stream failed';
    if(mode===6 || mode===7)message.usage={...usage,cost:{...usage.cost}};
    if(mode===13){message.providerThinkingLevel='native-high';message.usage.totalTokens=42;}
    stream.push({type:'error',reason:message.stopReason,error:message});return stream;
   }
   stream.push({type:'start',partial:assistant([{type:'text',text:'thinking...'}])});
   const call=requests===1 && mode!==9;
   const content=call?[{type:'toolCall',id:'call-1',name:mode===1?'missing':'echo',arguments:{}}]:[{type:'text',text:'done'}];
   const reason=call?(mode===4?'length':'toolUse'):'stop';
   stream.push({type:'done',reason,message:assistant(content,reason)});return stream;
  });
 const native=JSON.parse(execFileSync(resolve(process.argv[2]),['--case',String(mode)],{encoding:'utf8'}));
 assert.equal(native.status,mode===6 || mode===11?5:mode===7 || mode>=12?4:0);assert.equal(native.effects,effects);assert.equal(native.requests,requests);
 assert.equal(native.finished,finished,`mode ${mode} finishTurn calls`);
 assert.deepEqual(native.events,events,`mode ${mode} lifecycle`);
 assert.deepEqual(native.request.context.messages.map(normalize),messages.map(normalize),`mode ${mode} messages`);
}
console.log('identical eleven complete normal/unknown/blocked/truncated/terminate/continue/error/abort scenarios');

if(process.argv[10]) {
 const {runToolCall}=await import(pathToFileURL(resolve(directory,'agent/agent-loop.ts')));
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 let nestedAgent,nestedRequests=0;const nestedPending=[];
 const inner={name:'inner',label:'inner',description:'',parameters:{},async execute(){
  nestedPending.push(nestedAgent.state.pendingToolCalls.size);return {content:[{type:'text',text:'inner'}],details:{}};
 }};
 const outer={name:'outer',label:'outer',description:'',parameters:{},async execute(id){
  nestedPending.push(nestedAgent.state.pendingToolCalls.size);
  const result=await runToolCall({type:'toolCall',id,name:'inner',arguments:{}},
   {tools:[inner],assistantMessage:nestedAgent.state.messages.at(-1),context:{messages:nestedAgent.state.messages,tools:[inner]}});
  assert.equal(result.result.content[0].text,'inner');nestedPending.push(nestedAgent.state.pendingToolCalls.size);
  return {content:[{type:'text',text:'outer'}],details:{}};
 }};
 nestedAgent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'},tools:[outer]},streamFn:()=>{
  const stream=createAssistantMessageEventStream();++nestedRequests;
  const content=nestedRequests===1?[{type:'toolCall',id:'shared',name:'outer',arguments:{}}]:[];
  const reason=content.length?'toolUse':'stop';stream.push({type:'done',reason,message:assistant(content,reason)});return stream;
 }});
 await nestedAgent.prompt('nested');assert.equal(nestedRequests,2);
 assert.equal(nestedAgent.state.messages.filter(m=>m.role==='toolResult').length,1);assert.equal(nestedAgent.state.pendingToolCalls.size,0);
 assert.equal(execFileSync(resolve(process.argv[10]),['--trace-nested-pending'],{encoding:'utf8'}).trim(),`nested-pending:${nestedPending.join(':')}`);
 console.log('identical outer pending Set retained across a same-ID standalone nested tool');
 const detailTraces=[];
 for(let stage=0;stage<6;++stage)for(const loop of [0,1]){
  const fallback=stage<2?'Invalid tool arguments':stage===2?'Tool preflight hook failed':stage===4?'Tool result hook failed':'Tool execution failed';
  const schemaFault=[!loop,!loop];
  const fail=index=>{throw new Error(index?fallback:'tool a exploded');};
  const tools=['a','b'].map((name,index)=>{
   const tool={name,label:name,description:'',parameters:{},
    prepareArguments:stage===0?()=>fail(index):undefined,
    async execute(){if(stage===3||stage===5)fail(index);return {content:[{type:'text',text:'ok'}],details:{}};}};
   // Inject a schema-access fault precisely at validation. Loop declaration
   // discovery remains valid; the fault is armed only after the first request.
   if(stage===1)Object.defineProperty(tool,'parameters',{get(){if(schemaFault[index]){schemaFault[index]=false;fail(index);}return {};}});
   return tool;
  });
  const hooks={beforeToolCall:stage===2?input=>fail(input.toolCall.id==='a'?0:1):undefined,
   afterToolCall:stage===4?input=>fail(input.toolCall.id==='a'?0:1):undefined};
  let results=[];
  if(loop){
   let requests=0;
   const transcript=await runAgentLoop([{role:'user',content:[],timestamp:0}],{messages:[],tools},
    {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m,...hooks},()=>{},undefined,()=>{
     const stream=createAssistantMessageEventStream();++requests;
     const calls=requests===1?tools.map(t=>({type:'toolCall',id:t.name,name:t.name,arguments:{}})):[];
     if(requests===1&&stage===1)schemaFault.fill(true);
     const reason=calls.length?'toolUse':'stop';stream.push({type:'done',reason,message:assistant(calls,reason)});return stream;
    });
   assert.equal(requests,2);results=transcript.filter(m=>m.role==='toolResult').map(m=>({result:m,isError:m.isError}));
  }else{
   for(const tool of tools)results.push(await runToolCall({type:'toolCall',id:tool.name,name:tool.name,arguments:{}},
    {tools,assistantMessage:assistant([]),context:{messages:[],tools},...hooks}));
  }
  assert.equal(results.length,2);assert.ok(results.every(r=>r.isError));
  detailTraces.push(`tool-diagnostic:${stage}:${loop}:${results.map(r=>r.result.content[0].text).join('|')}`);
 }
 assert.deepEqual(execFileSync(resolve(process.argv[10]),['--trace-diagnostics'],{encoding:'utf8'}).trim().split('\n'),detailTraces);
 console.log('identical six tool-stage exception diagnostics, standalone and two-tool source-order results');
 const beforeTrace=[];
 let requests=0;
 const tool={name:'tool',label:'tool',description:'',parameters:{type:'object',properties:{value:{type:'string'}},required:['value']},
  async execute(_id,args){beforeTrace.push('execute:'+JSON.stringify(args));return {content:[{type:'text',text:'result'}],details:{}};}};
 await runAgentLoop([{role:'user',content:[],timestamp:0}],{messages:[],tools:[tool]},
  {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m,
   beforeToolCall:context=>{beforeTrace.push('raw:'+JSON.stringify(context.toolCall.arguments));context.args.value=123;},
   afterToolCall:context=>{beforeTrace.push('after:'+JSON.stringify(context.args));}},
  ()=>{},undefined,()=>{
   const stream=createAssistantMessageEventStream();
   const content=requests++?[]:[{type:'toolCall',id:'mutable',name:'tool',arguments:{value:'hello'}}];
   const reason=content.length?'toolUse':'stop';stream.push({type:'done',reason,message:assistant(content,reason)});return stream;
  });
 const nativeBefore=execFileSync(resolve(process.argv[10]),['--trace-before'],{encoding:'utf8'}).trim().split('\n');
 assert.deepEqual(nativeBefore,beforeTrace);
 console.log('identical raw/execute/after argument views after beforeToolCall mutation without schema revalidation');
}

// Permanently replace the earlier build/pi-diff/check.mjs manual comparison.
if(process.argv[9]) {
 const native=execFileSync(resolve(process.argv[9]),{encoding:'utf8'}).trim().split('\n');
 for(const mode of [0,1,2,3,4,7,8,10,11,12,13,14]) {
  let trace='',requests=0,queued=false,delivered=false,completed=0;
  const releases={},controller=new AbortController();
  const tool={name:'tool',label:'tool',description:'tool',parameters:{},
   ...(mode===2?{executionMode:'sequential'}:{}),
   async execute(id){
    trace+=id.toUpperCase();
    if(mode===12||mode===13)queued=true;
    if(![1,2,10,13].includes(mode) && id!=='c')await new Promise(resolve=>releases[id]=resolve);
    return {content:[{type:'text',text:'completed'}],details:{},terminate:mode===7 || (mode===8 && id!=='b')};
   }};
  const tools=mode>=10?[tool,{...tool,name:mode===14?'tool':'other',executionMode:mode===10||mode===14?'sequential':'parallel'}]:[tool];
  await runAgentLoop([{role:'user',content:'go',timestamp:0}],{messages:[],tools},
   {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m,toolExecution:mode===1||mode===13?'sequential':'parallel',
    getSteeringMessages:()=>{
     if(!queued||delivered)return [];
     assert.equal(completed,3);delivered=true;
     return [{role:'user',content:'interrupt',timestamp:0}];
    },
    beforeToolCall:({toolCall})=>{trace+=toolCall.id;if(mode===4 && toolCall.id==='b')controller.abort();return {block:mode===3 && toolCall.id==='b'};}},
   event=>{
    if(event.type==='tool_execution_end'){
     ++completed;
     trace+=String(event.toolCallId.charCodeAt(0)-96);
     if(event.toolCallId==='c'){if(releases.b)releases.b();else releases.a?.();}
     if(event.toolCallId==='b')releases.a?.();
    }
    if(event.type==='message_end' && event.message.role==='toolResult')trace+=String.fromCharCode(event.message.toolCallId.charCodeAt(0)+23);
    if((mode===12||mode===13)&&event.type==='message_start'&&event.message.role==='user'&&event.message.content==='interrupt'){
     assert.equal(completed,3);trace+='q';
    }
   },controller.signal,()=>{
    const stream=createAssistantMessageEventStream();
    const content=++requests===1?['a','b','c'].map(id=>({type:'toolCall',id,name:mode>=10&&mode<=13&&id==='b'?'other':'tool',arguments:{}})):[];
    const message=assistant(content,requests===1?'toolUse':controller.signal.aborted?'aborted':'stop');
    stream.push(message.stopReason==='aborted'?{type:'error',reason:'aborted',error:message}:{type:'done',reason:message.stopReason,message});return stream;
   });
  assert.equal(native.find(line=>line.startsWith(`${mode}:`)),`${mode}:${trace}`,`parallel/sequential mode ${mode}`);
  assert.equal(requests,mode===7?1:2);
 }
 console.log('identical twelve scheduling traces including first-definition lookup and in-flight queue delivery');
}

// A provider failing before START must still identify the requested model.
{
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const expected=[];
 for(let mode=0;mode<2;++mode) {
  const agent=new Agent({initialState:{model:{id:'configured',api:'api',provider:'provider'}},streamFn:()=>{
   if(mode)agent.abort();throw Error(mode?'Operation aborted':'Model stream failed');
  }});
  await agent.prompt('fail');
  const message=agent.state.messages.at(-1);
  expected.push(`failure:${mode}:${message.model}:${message.provider}:${message.api}:${message.stopReason==='aborted'?5:4}`);
 }
 const native=execFileSync(resolve(process.argv[2]),['--trace-failure-identity'],{encoding:'utf8'}).trim().split('\n');
 assert.deepEqual(native.slice(0,2),expected);
 console.log('identical requested model identity on failure/abort before first provider snapshot');
}

// Agent handles preparation exceptions outside the low-level loop lifecycle.
{
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const expected=[];
 const kinds=['agent_start','agent_end','turn_start','turn_end','message_start','message_update','message_end'];
 for(let mode=0;mode<4;++mode) {
  let agent;
  const fail=()=>{if(mode&1)agent.abort();throw Error(mode&1?'Operation aborted':'Request preparation failed');};
  agent=new Agent({initialState:{model:{id:'configured',api:'api',provider:'provider'}},
   ...(mode&2?{prepareNextTurn:fail,finishTurn:()=>({action:'continue'})}:{prepareRequest:fail}),
   streamFn:()=>{
    const stream=createAssistantMessageEventStream();
    const message=assistant([],'stop');
    stream.push({type:'done',reason:'stop',message});return stream;
   }});
  const events=[];agent.subscribe(event=>events.push(kinds.indexOf(event.type)));
  await agent.prompt('fail');
  const message=agent.state.messages.at(-1);
  assert.equal(message.errorMessage,mode&1?'Operation aborted':'Request preparation failed');
  expected.push(`prep:${mode}:${events.join(',')}:${agent.state.messages.length}:${message.stopReason==='aborted'?5:4}`);
 }
 const native=execFileSync(resolve(process.argv[2]),['--trace-preparation'],{encoding:'utf8'}).trim().split('\n');
 assert.deepEqual(native,expected);
 console.log('identical Agent preparation failure/abort lifecycle before request and between turns');
}

// Pre-provider hook exceptions bypass finishTurn and leave converted data private.
{
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const errors=['Context transform failed','Message conversion failed','API key resolution failed'];
 const expected=[];
 const kinds=['agent_start','agent_end','turn_start','turn_end','message_start','message_update','message_end'];
 for(let stage=0;stage<3;++stage)for(let mode=0;mode<2;++mode){
  let agent,finished=0,requests=0;
  const fail=()=>{if(mode)agent.abort();throw Error(mode?'Operation aborted':errors[stage]);};
  const hooks=stage===0?{transformContext:fail}:stage===1?{convertToLlm:fail}:{getApiKey:fail};
  agent=new Agent({initialState:{model:{id:'configured',provider:'provider',api:'api'}},...hooks,
   finishTurn:()=>{++finished;},streamFn:()=>{++requests;throw Error('unexpected provider');}});
  const events=[];agent.subscribe(event=>events.push(kinds.indexOf(event.type)));
  await agent.prompt('fail');
  const message=agent.state.messages.at(-1);
  assert.equal(message.errorMessage,mode?'Operation aborted':errors[stage]);
  assert.equal(finished,0);assert.equal(requests,0);
  expected.push(`hook:${stage}:${mode}:${events.join(',')}:${agent.state.messages.length}:${message.stopReason==='aborted'?5:4}`);
 }
 const native=execFileSync(resolve(process.argv[2]),['--trace-request-hooks'],{encoding:'utf8'}).trim().split('\n');
 assert.deepEqual(native,expected);
 console.log('identical transform/conversion/credential exception and abort lifecycle, no finishTurn/provider');
}

if(process.argv[10]) {
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 let calls=0;const messageTimes=[];
 const timestampTool={name:'tool',label:'tool',description:'',parameters:{},
  async execute(){return {content:[{type:'text',text:'result'}],details:{},timestamp:123};}};
 const timestampAgent=new Agent({initialState:{model:{id:'mock',api:'mock',provider:'mock'},tools:[timestampTool]},
  streamFn:()=>{const stream=createAssistantMessageEventStream();const first=calls++===0;
   const message=assistant(first?[{type:'toolCall',id:'one',name:'tool',arguments:{}}]:[],first?'toolUse':'stop');
   stream.push({type:'done',reason:message.stopReason,message});return stream;}});
 timestampAgent.subscribe(e=>{if((e.type==='message_start'||e.type==='message_end')&&e.message.role==='toolResult')messageTimes.push(e.message.timestamp);});
 await timestampAgent.prompt('timestamp');
 const savedTime=timestampAgent.state.messages.find(m=>m.role==='toolResult').timestamp;
 assert.deepEqual(JSON.parse(execFileSync(resolve(process.argv[10]),['--trace-timestamp'],{encoding:'utf8'})),
  {fresh:savedTime>123,stable:messageTimes.length===2&&messageTimes.every(t=>t===savedTime)});
 console.log('identical fresh tool-result timestamp retained through events and next model context');
 {
  let prepared=0,before=0,executed=0,after=0,requests=0;
  const tool={name:'tool',label:'tool',description:'',parameters:{type:'object',properties:{new:{type:'boolean'}},required:['new']},
   prepareArguments:args=>{++prepared;assert.deepEqual(args,{old:true});return {new:true};},
   async execute(_id,args){++executed;assert.deepEqual(args,{new:true});return {content:[{type:'text',text:'original'}],details:{}};}};
  const messages=await runAgentLoop([{role:'user',content:'run',timestamp:0}],{messages:[],tools:[tool]},
   {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m,
    beforeToolCall:input=>{++before;assert.deepEqual(input.toolCall.arguments,{old:true});assert.deepEqual(input.args,{new:true});},
    afterToolCall:input=>{++after;assert.deepEqual(input.args,{new:true});}},async()=>{},undefined,()=>{
    const stream=createAssistantMessageEventStream(),first=requests++===0;
    const message=assistant(first?[{type:'toolCall',id:'one',name:'tool',arguments:{old:true}}]:[],first?'toolUse':'stop');
    stream.push({type:'done',reason:message.stopReason,message});return stream;
   });
  const raw=messages.find(m=>m.role==='assistant'&&m.content[0]?.type==='toolCall').content[0];
  assert.deepEqual(JSON.parse(execFileSync(resolve(process.argv[10]),['--trace-preparation'],{encoding:'utf8'})),
   {prepared,before,executed,after,requests,rawUnchanged:JSON.stringify(raw.arguments)==='{"old":true}'});
  console.log('identical argument preparation before Schema validation, prepared hook/executor views and unchanged raw call');
 }
 {
  const descriptor=Object.getOwnPropertyDescriptor(globalThis,'performance');
  let clock=0,effects=0,requests=0,eventDuration;
  Object.defineProperty(globalThis,'performance',{configurable:true,value:{now:()=>clock}});
  try{
   const tool={name:'tool',label:'tool',description:'',parameters:{},async execute(){
    ++effects;clock+=26.5;return {content:[{type:'text',text:'original'}],details:{}};
   }};
   const messages=await runAgentLoop([{role:'user',content:'run',timestamp:0}],{messages:[],tools:[tool]},
    {model:{id:'mock',api:'mock',provider:'mock'},convertToLlm:m=>m,
     beforeToolCall:()=>{clock+=100;},afterToolCall:()=>{clock+=500;}},
    e=>{if(e.type==='tool_execution_end')eventDuration=e.durationMs;},undefined,()=>{
     const stream=createAssistantMessageEventStream(),first=requests++===0;
     const message=assistant(first?[{type:'toolCall',id:'one',name:'tool',arguments:{}}]:[],first?'toolUse':'stop');
     stream.push({type:'done',reason:message.stopReason,message});return stream;
    });
   const result=messages.find(m=>m.role==='toolResult');assert.equal(requests,2);
   assert.deepEqual(JSON.parse(execFileSync(resolve(process.argv[10]),['--trace-duration'],{encoding:'utf8'})),
    {eventDuration,resultDuration:result.durationMs,effects});
  }finally{if(descriptor)Object.defineProperty(globalThis,'performance',descriptor);else delete globalThis.performance;}
  console.log('identical execute-only duration rounding in tool-end event and persisted result, excluding before/after hooks');
 }
 const {runToolCall}=await import(pathToFileURL(resolve(directory,'agent/agent-loop.ts')));
 {
  let prepared=0,executed=0,before=0;
  const tool={name:'tool',label:'tool',description:'',parameters:{},
   prepareArguments:()=>{++prepared;return {new:true};},async execute(){++executed;return {content:[],details:{}};}};
  const outcome=await runToolCall({type:'toolCall',id:'one',name:'tool',arguments:{old:true},argumentsError:'truncated JSON'},
   {tools:[tool],assistantMessage:assistant([]),context:{messages:[],tools:[tool]},
    beforeToolCall:()=>{++before;}});
  assert.equal(prepared,1);assert.equal(before,0);assert.equal(executed,0);assert.equal(outcome.isError,true);
  assert.equal(execFileSync(resolve(process.argv[10]),['--trace-argument-error'],{encoding:'utf8'}).trim(),outcome.result.content[0].text);
  console.log('identical argument-error preparation order and full tool-name/provider-parse diagnostic');
 }
 const expected=[];
 for(let mode=0;mode<=13;++mode) {
  if(mode===8)continue; // Detailed Schema diagnostics have a separate native adapter.
  let prepared=0,before=0,executed=0,after=0,updates=0,late;
  const controller=new AbortController();
  const original={content:[{type:'text',text:'original'}],details:{note:'original'},
   structuredContent:{value:'original'},usage:{...usage,totalTokens:7},isError:true,terminate:true};
  const tool={name:'tool',label:'tool',description:'',parameters:{},
   prepareArguments:()=>{++prepared;if(mode===9)throw Error('Invalid tool arguments');return {new:true};},
   async execute(_id,args,_signal,onUpdate){
    ++executed;assert.deepEqual(args,{new:true});
    if(mode===11)throw Error('Tool execution failed');
    onUpdate(original);late=onUpdate;return original;
   }};
  const outcome=await runToolCall({type:'toolCall',id:'one',name:mode===10?'missing':'tool',arguments:{old:true}},
   {tools:[tool],assistantMessage:assistant([],'length'),context:{messages:[],tools:[tool]},signal:controller.signal,
    onUpdate:()=>{++updates;},beforeToolCall:()=>{++before;if(mode===13)controller.abort();return {block:mode===7,reason:'nope',terminate:true};},
    afterToolCall:()=>{
     ++after;
     if(mode===12)throw Error('Tool result hook failed');
     if(mode===1)return {content:[{type:'text',text:'redacted'}]};
     if(mode===2)return {structuredContent:{value:'replaced'}};
     if(mode===3)return {content:[{type:'text',text:'redacted'}],structuredContent:{value:'replaced'}};
     if(mode===4)return {details:{note:'kept'},usage:{...usage,totalTokens:12}};
     if(mode===5)return {isError:false,terminate:false};
     if(mode===6)return {content:[]};
    }});
  late?.(original); // Settled tool updates are ignored.
  const result=outcome.result;
  expected.push({mode,text:result.content[0]?.text??'',contentCount:result.content.length,
   structured:result.structuredContent??null,details:result.details,usage:result.usage?.totalTokens??0,
   error:+outcome.isError,terminate:+(result.terminate===true),timed:+('durationMs' in outcome),prepared,before,executed,after,updates});
 }
 const native=execFileSync(resolve(process.argv[10]),['--trace-outcomes'],{encoding:'utf8'}).trim().split('\n')
  .filter(line=>line.startsWith('outcome:')).map(line=>JSON.parse(line.slice(8)));
 assert.deepEqual(native,expected);
 console.log('identical thirteen standalone tool outcomes: patches, block, cancellation, callback failures and late updates');
}

// A provider may report cancellation independently of the host abort signal.
{
 const {Agent}=await import(pathToFileURL(resolve(directory,'agent/agent.ts')));
 const expected=[];
 for(let mode=4;mode<6;++mode){
  const partial=mode===5;
  const identity=partial?{id:'actual-model',provider:'actual-provider',api:'actual-api'}:{id:'configured',provider:'provider',api:'api'};
  const agent=new Agent({initialState:{model:{id:'configured',provider:'provider',api:'api'}},streamFn:()=>{
   const stream=createAssistantMessageEventStream();
   const message={...assistant([{type:'text',text:partial?'retained partial':'Operation aborted'}],'aborted'),
    model:identity.id,provider:identity.provider,api:identity.api,errorMessage:'Operation aborted'};
   if(partial)stream.push({type:'start',partial:{...message,stopReason:'pending'}});
   stream.push({type:'error',reason:'aborted',error:message});return stream;
  }});
  await agent.prompt('cancelled provider');
  const message=agent.state.messages.at(-1);
  expected.push(`failure:${mode}:${message.model}:${message.provider}:${message.api}:${message.stopReason==='aborted'?5:4}`);
 }
 const native=execFileSync(resolve(process.argv[2]),['--trace-failure-identity'],{encoding:'utf8'}).trim().split('\n');
 assert.deepEqual(native.slice(4),expected);
 console.log('identical independent provider cancellation before/after partial without host abort');
}
