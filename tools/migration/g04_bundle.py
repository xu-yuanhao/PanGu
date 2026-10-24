"""Fixed local G04 experiment bundle; does not grant full G04 or release admission."""
import json
from pathlib import Path
import sys
import g04_async_trace
import g04_branch_trace
import g04_composition
import g04_interleave

HEADERS = ('adapter_public_scenarios=675 valid_plans=95 rejected_plans=145 public_dependency_calls=86 no_edge_owner_only_holds=106', 'async_setup_core_scenarios=1414 valid_plans=95 rejected_plans=145 invalid_ticket_refusals=3636 inline_finishes=202 async_cleanup_finishes=202 async_disposer_public_groups=7 rejected_cleanup_tickets=8 inline_cleanup_finishes=2 budget_public_cases=2736 bounded_ticks=44904 zero_budget_owner_work=9 public_call_skips=436 prefix_cases=16 call_budget_cases=207 admitted_calls=207 insufficient_budget_retries=1191 quota_refusals=30 invalid_bindings=30 overlap_refusals=345 setup_contexts=202 cleanup_contexts=202 call_lifecycle_cases=207 direct_calls=69 setup_calls=819 cleanup_calls=819 provider_invocations=1776 rejected_calls=7212 async_cleanup_begins=69 geometry_cases=98 geometry_matrices=600 geometry_refusals=143961 graph_cases=516 independent_cases=326 ancestor_cases=224 independent_setup_cases=83 error_while_waiting=44 edge_prepare_cases=98 call_hold_cases=112 private_owner_hold_skips=292 start_budget_cases=6056 start_ticks=103692 pre_setup_cancellations=285 invalid_before_callbacks=114 transactions=98 refused_plans=145 transaction_alias_rejections=6432 transaction_quota_rejections=12127 earlier_owner_groups=4')
SUITES = (("async",50,g04_async_trace.validate),("branch",228,g04_branch_trace.validate),
          ("composition",24,g04_composition.validate),("interleaving",72,g04_interleave.validate))

def validate(data):
 if type(data) is not bytes or not 0<len(data)<=1048576:raise ValueError("bundle byte bound")
 text=data.decode("utf-8");lines=text.splitlines()
 if not text.endswith("\n") or len(lines)!=376:raise ValueError("bundle record count/terminator")
 if tuple(lines[:2])!=HEADERS:raise ValueError("frozen regression summary mismatch")
 result={};offset=2
 for name,count,validator in SUITES:
  result[name]=validator(("\n".join(lines[offset:offset+count])+"\n").encode());offset+=count
 if offset!=len(lines):raise ValueError("extra bundle records")
 return {"status":"PASS_LOCAL_BUNDLE","suite_count":4,"records":374,"suites":result,
         "full_G04":"NOT_COMPLETED","production_admission":"NOT_GRANTED"}

def main(argv):
 if len(argv)!=1:
  print("usage: g04_bundle.py STDOUT_FILE",file=sys.stderr);return 2
 try:
  path=Path(argv[0])
  with path.open("rb") as stream:data=stream.read(1048577)
  result=validate(data)
 except (OSError,ValueError,UnicodeError,RecursionError) as error:
  print(json.dumps({"status":"BLOCKED","error":str(error)}));return 2
 print(json.dumps(result));return 0
if __name__=="__main__":sys.exit(main(sys.argv[1:]))
