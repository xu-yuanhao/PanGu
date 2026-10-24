"""Local Pending/MANY phase snapshots; not full per-tick G04 traces."""
import json
FIELDS=set('case q many event rc phase started setups cleanups ticks work max_work state refs resources'.split())
def require(ok,message):
 if not ok:raise ValueError(message)
def pairs(items):
 d={}
 for k,v in items:require(k not in d,'duplicate key');d[k]=v
 return d
def validate(data):
 require(type(data) is bytes and 0<len(data)<=65536,'byte bound');rows=[json.loads(x,object_pairs_hook=pairs) for x in data.decode('utf-8').splitlines()];require(len(rows)==24,'event count');offset=0
 for case in range(1,9):
  q=(case-1)//2+1;many=(case-1)%2;events=['active','waiting','partial','final'] if many else ['active','final'];lastticks=lastwork=0
  for event in events:
   r=rows[offset];offset+=1;require(type(r) is dict and set(r)==FIELDS,'fields')
   require(all(type(r[k]) is int for k in FIELDS-{'event','state','refs','resources'}) and type(r['event']) is str,'types')
   for k in ('state','refs','resources'):require(type(r[k]) is list and len(r[k])==4 and all(type(x) is int for x in r[k]),'array types')
   require((r['case'],r['q'],r['many'],r['event'])==(case,q,many,event),'order')
   require(r['ticks']>=lastticks and r['work']>=lastwork and 0<=r['max_work']<=q and r['work']<=r['ticks']*q,'budget totals');lastticks,lastwork=r['ticks'],r['work']
   if event=='active':require(r['ticks']==r['work']==r['max_work']==0,'active budget')
   else:require(r['ticks']>0 and r['work']>0 and r['max_work']>0,'no bounded work')
   if many:
    spec={'active':(0,2,4,0,[2,2,2,2],[1,1,2,0],[1,1,1,1]),'waiting':(5,3,3,1,[2,2,3,4],[1,1,2,0],[1,1,1,0]),'partial':(5,3,3,1,[2,2,3,4],[0,1,1,0],[1,1,1,0]),'final':(0,4,0,4,[4]*4,[0]*4,[0]*4)}[event];setups=4
   else:
    spec={'active':(0,2,3,0,[2,2,1,2],[0]*4,[1,1,0,1]),'final':(0,4,0,3,[4,4,1,4],[0]*4,[0]*4)}[event];setups=3
   require((r['rc'],r['phase'],r['started'],r['cleanups'],r['state'],r['refs'],r['resources'])==spec and r['setups']==setups,'lifecycle snapshot')
 return {'status':'PASS','cases':8,'snapshots':24}
