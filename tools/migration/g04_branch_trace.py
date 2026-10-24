"""Local fixed graph trace invariants, not full G04 or planner validation."""
import json
FIELDS=set('case seq q fault event owner rc phase started work pending reason error state refs resources disposed'.split())
def require(ok,message):
 if not ok:raise ValueError(message)
def pairs(items):
 d={}
 for k,v in items:require(k not in d,'duplicate key');d[k]=v
 return d
def validate(data):
 require(type(data) is bytes and 0<len(data)<=1048576,'byte limit')
 rows=[json.loads(x,object_pairs_hook=pairs) for x in data.decode('utf-8').splitlines()];require(len(rows)<=4096,'record limit');offset=0;total_ticks=0
 for case in range(1,9):
  stage='begin';cleaned=set();previous_disposed=[0]*4;seq=0;ticks=0;held_snapshot=None
  while True:
   require(offset<len(rows),'missing event');r=rows[offset];offset+=1
   require(type(r) is dict and set(r)==FIELDS,'fields')
   require(all(type(r[k]) is int for k in FIELDS-{'event','state','refs','resources','disposed'}) and type(r['event']) is str,'types')
   for k in ('state','refs','resources','disposed'):require(type(r[k]) is list and len(r[k])==4 and all(type(x) is int for x in r[k]),'array types')
   q=(case-1)//2+1;fault=-33 if case%2==0 else 0
   require((r['case'],r['seq'],r['q'],r['fault'])==(case,seq,q,fault),'case/sequence');seq+=1
   require(0<=r['work']<=q and r['phase'] in range(7) and r['pending'] in (0,1) and r['reason'] in (0,1,2),'report bounds')
   require(all(x in (1,2,3,4) for x in r['state']) and all(x in (0,1) for x in r['refs']+r['resources']+r['disposed']),'owner bounds')
   require(all(a<=b for a,b in zip(previous_disposed,r['disposed'])),'disposed regressed');previous_disposed=r['disposed']
   require(r['started']==4-sum(r['disposed']),'started count')
   require(all(not d or (s==4 and not ref and not res) for d,s,ref,res in zip(r['disposed'],r['state'],r['refs'],r['resources'])),'disposed owner alive')
   require(r['error'] in (0,fault) and (3 in cleaned or not r['error']),'error provenance')
   e=r['event'];rc=r['rc']
   if stage in ('begin','zero','held','independent') and r['event']!='finish':
    require(r['refs']==[1,1,0,0],'held endpoints lost')
    require(r['state'][0]==2 and not r['disposed'][0] and not r['disposed'][1],'provider released while held')
   else:require(r['refs']==[0,0,0,0],'references after finish')
   if e=='cleanup':
    require(stage in ('held','drain'),'cleanup stage');i=r['owner'];require(i in range(4) and i not in cleaned,'duplicate cleanup')
    require(stage=='drain' or i in (2,3),'held branch cleaned')
    require(r['state'][i]==3 and not r['resources'][i] and not r['refs'][i],'cleanup frame')
    if i in (0,2):require(i+1 in cleaned,'provider before consumer')
    if i in (1,3):require(r['state'][i-1]==2,'provider not active in cleanup')
    require(rc==(fault if i==3 else 0),'cleanup status');cleaned.add(i)
   elif e=='held':
    require(stage=='begin' and rc==0 and r['phase']==2 and r['state']==[2]*4 and r['resources']==[1]*4,'initial hold');held_snapshot={k:r[k] for k in ('phase','started','error','state','refs','resources','disposed')};stage='zero'
   elif e=='zero':
    require(stage=='zero' and rc==2 and r['work']==0 and r['pending']==1 and r['reason']==1,'zero budget');require(all(r[k]==v for k,v in held_snapshot.items()),'zero budget mutation');stage='held'
   elif e in ('tick-held','tick-drain'):
    require(stage==('held' if e=='tick-held' else 'drain'),'tick stage');ticks+=1;total_ticks+=1;require(ticks<512,'tick bound')
    if r['pending']:require((rc==2 and r['reason']==1) or (e=='tick-held' and rc==5 and r['reason']==2),'pending status')
    else:require(e=='tick-drain' and rc==fault and r['reason']==0 and r['phase']==4,'terminal tick')
   elif e=='independent-drained':
    require(stage=='held' and rc==5 and r['disposed']==[0,0,1,1] and cleaned=={2,3} and r['state']==[2,3,4,4] and r['error']==fault,'independent branch did not drain');stage='independent'
   elif e=='finish':require(stage=='independent' and rc==0,'finish');stage='duplicate'
   elif e=='duplicate':require(stage=='duplicate' and rc==10,'duplicate finish');stage='drain'
   elif e=='end':
    require(stage=='drain' and cleaned=={0,1,2,3} and r['disposed']==[1]*4 and r['state']==[4]*4 and r['resources']==[0]*4 and rc==fault and r['error']==fault and r['phase']==4,'final drain');break
   else:raise ValueError('unknown event')
   if e!='cleanup':require(r['owner']==-1,'event owner')
  require(ticks>0,'no budget steps')
 require(offset==len(rows),'extra event')
 return {'status':'PASS','cases':8,'events':len(rows),'ticks':total_ticks}
