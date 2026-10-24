"""Local three-case async lifecycle oracle; not a production G04 gate."""
import json
FIELDS=set('case seq event rc phase started stop load error state inflight resources waiting begins'.split())
def require(ok,message):
 if not ok:raise ValueError(message)
def pairs(items):
 d={}
 for k,v in items:
  require(k not in d,'duplicate key');d[k]=v
 return d
def validate(data):
 require(type(data) is bytes and 0<len(data)<=65536,'trace byte bound')
 rows=[json.loads(line,object_pairs_hook=pairs) for line in data.decode('utf-8').splitlines()]
 require(len(rows)<=96,'event bound');offset=0
 for case in (1,2,3):
  grammar=[('case-begin',0),('setup-begin',0),('setup-deferred',0),('start-wait',5),('pump-wait',5)]
  if case==2:grammar += [('cancel-stop',5)]
  grammar += [('setup-finish',0),('setup-duplicate',10)]
  if case==1:grammar += [('pump-after-setup',0),('cleanup-begin',0),('stop-wait',5)]
  else:grammar += [('cleanup-begin',0),('pump-after-setup',5)]
  grammar += [('cleanup-wait-again',5),('cleanup-forged',10),('cleanup-finish',0),('cleanup-duplicate',10),('pump-final',-71 if case==3 else 6),('stop-final',-22 if case==3 else 0),('case-end',0)]
  state=dict(phase=0,started=0,stop=0,load=0,error=0,state=1,inflight=0,resources=0,waiting=0,begins=0)
  for seq,(event,rc) in enumerate(grammar):
   require(offset<len(rows),'missing event');row=rows[offset];offset+=1
   require(type(row) is dict and set(row)==FIELDS,'fields')
   require(type(row['event']) is str and all(type(v) is int for k,v in row.items() if k!='event'),'field types')
   require((row['case'],row['seq'],row['event'],row['rc'])==(case,seq,event,rc),'event order/status')
   if event=='setup-begin':state.update(phase=1,started=1,inflight=1)
   if event=='setup-deferred':state['resources']=1
   if event=='cancel-stop':state.update(stop=1,state=3)
   if event=='pump-after-setup' and case==1:state.update(phase=2,state=2,inflight=0)
   if event=='cleanup-begin':state.update(phase=3,stop=1,state=3,inflight=0,resources=0,waiting=1,begins=1,load=-71 if case==3 else 0)
   if event=='pump-final':state.update(phase=4,started=0,state=4,waiting=0,error=-22 if case==3 else 0)
   require(all(row[k]==v for k,v in state.items()),f'state mismatch case{case} {event}')
  require(state['state']==4 and not state['inflight'] and not state['resources'],'terminal drain')
 require(offset==len(rows),'extra event')
 return {'cases':3,'events':len(rows),'status':'PASS'}
