"""Three legal completion schedules, fixed topology phase snapshots only."""
import json
FIELDS=set('case q order event rc phase started error steps work max_work state refs resources waiting begins disposed'.split())
ARRAYS={'state','refs','resources','waiting','begins','disposed'}
def require(ok,message):
 if not ok:raise ValueError(message)
def pairs(items):
 d={}
 for k,v in items:require(k not in d,'duplicate key');d[k]=v
 return d
def validate(data):
 require(type(data) is bytes and 0<len(data)<=65536,'byte bound');rows=[json.loads(x,object_pairs_hook=pairs) for x in data.decode('utf-8').splitlines()];require(len(rows)==72,'record count');offset=0
 schedules=[['call-finished','right-finished','left-finished'],['call-finished','left-finished','right-finished'],['right-finished','call-finished','left-finished']]
 for case in range(1,13):
  q=(case-1)//3+1;order=(case-1)%3;done=set();error=0;previous=None
  for event in ['active','waiting']+schedules[order]+['final']:
   r=rows[offset];offset+=1;require(type(r) is dict and set(r)==FIELDS,'fields');require(type(r['event']) is str and all(type(r[k]) is int for k in FIELDS-ARRAYS-{'event'}),'types')
   for k in ARRAYS:require(type(r[k]) is list and len(r[k])==4 and all(type(x) is int for x in r[k]),'array types')
   require((r['case'],r['q'],r['order'],r['event'])==(case,q,order,event),'schedule')
   if event.endswith('-finished'):
    done.add(event)
    if event!='call-finished' and not error:error=-33 if event=='right-finished' else -11
   require(0<=r['max_work']<=q and 0<=r['work']<=r['steps']*q,'budget')
   if event=='active':
    expected=dict(rc=0,phase=2,started=4,error=0,state=[2]*4,refs=[1,1,0,0],resources=[1]*4,waiting=[0]*4,begins=[0]*4,disposed=[0]*4);require(r['steps']==r['work']==r['max_work']==0,'active work')
   else:
    call='call-finished' in done;left='left-finished' in done;right='right-finished' in done;disposed=[int(left),int(left),int(right),int(right)];terminal=left and right
    expected=dict(rc=error if terminal else 5,phase=4 if terminal else 3,started=4-sum(disposed),error=error,state=[4 if left else 2,4 if left else 3,4 if right else 2,4 if right else 3],refs=[0 if call else 1,0 if call else 1,0,0],resources=[0 if left else 1,0 if call else 1,0 if right else 1,0],waiting=[0,int(call and not left),0,int(not right)],begins=[0,int(call),0,1],disposed=disposed)
    require(r['steps']>0 and r['work']>0 and r['max_work']>0,'no bounded work')
    if event=='final':require(all(r[k]==previous[k] for k in ('steps','work','max_work')),'final counters')
    else:require(r['steps']>previous['steps'] and r['work']>=previous['work'] and r['max_work']>=previous['max_work'],'progress counters')
   require(all(r[k]==v for k,v in expected.items()),f'snapshot {case} {event}');previous=r
 return {'status':'PASS','cases':12,'snapshots':72,'legal_completion_orders':3}
