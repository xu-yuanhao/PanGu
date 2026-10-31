"""Exercise the actual product composition resource service; --live uses a model."""
from pathlib import Path
import os
import argparse
import subprocess
import tempfile
from build_pangu import ROOT, SOURCES, PLUGIN_EXPORTS
from schema_dependency import compile_inputs

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live',action='store_true')
    parser.add_argument('--live-cost',action='store_true')
    parser.add_argument('--stored-auth',action='store_true')
    parser.add_argument('--live-agent',action='store_true')
    parser.add_argument('--live-conversations',action='store_true')
    parser.add_argument('--live-process-conversations',action='store_true')
    parser.add_argument('--live-extensions',action='store_true')
    parser.add_argument('--compaction-hooks',action='store_true')
    parser.add_argument('--native-tasks',action='store_true')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--live-compaction',action='store_true')
    parser.add_argument('--live-auto-compaction',action='store_true')
    parser.add_argument('--live-background-compaction',action='store_true')
    parser.add_argument('--live-dynamic-context',action='store_true')
    args=parser.parse_args()
    if args.stored_auth: os.environ['PANGU_TEST_STORED_AUTH']='1'
    with tempfile.TemporaryDirectory(prefix='pangu-product-resource-test-') as directory:
        output=Path(directory)/'test-product-resources'
        flags=subprocess.check_output(['pkg-config','--cflags','--libs','libcurl', 'libcrypto', 'glib-2.0', 'libzstd','libevent','json-c','libutf8proc','sqlite3','libgit2','yaml-0.1','libpcre2-8','vips'],text=True).split()
        sanitize=['-fsanitize=address,undefined','-fno-sanitize-recover=all'] if args.sanitize else []
        sources=compile_inputs([name for name in SOURCES if name!='src/composition/pangu.c'],directory,sanitize)
        subprocess.run(['/usr/bin/clang-23','-D_POSIX_C_SOURCE=200809L','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-O2',*sanitize,*PLUGIN_EXPORTS,*sources,str(ROOT/'tests/runtime/test_product_resources.c'),*flags,'-ldl','-o',str(output)],check=True)
        if not any((args.live_cost,args.live,args.live_agent,args.live_conversations,args.live_process_conversations,args.live_extensions,args.live_compaction,args.live_auto_compaction,args.live_background_compaction,args.live_dynamic_context)):
            import threading,json,base64
            from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
            from urllib.parse import parse_qs
            requests=[]
            device_requests=[];device_polls={};browser_requests=[];copilot_requests=[]
            payload=base64.urlsafe_b64encode(json.dumps({'https://api.openai.com/auth':{'chatgpt_account_id':'pangu-oauth-account'},'padding':'\uffff😀'},ensure_ascii=False).encode()).decode().rstrip('=')
            class OAuthReceiver(BaseHTTPRequestHandler):
                def log_message(self,*args): pass
                def do_GET(self):
                    prefix,action=self.path.rsplit('/',1);mode=int(prefix.removeprefix('/copilot'));copilot_requests.append((mode,action));status=200
                    assert self.headers['User-Agent']=='GitHubCopilotChat/0.35.0' and self.headers['Editor-Version']=='vscode/1.107.0' and self.headers['Editor-Plugin-Version']=='copilot-chat/0.35.0' and self.headers['Copilot-Integration-Id']=='vscode-chat'
                    if action=='token':
                        assert self.headers['Authorization']=='Bearer private-github-token'
                        host='proxy.team.githubcopilot.com' if mode==2 else 'proxy.individual.githubcopilot.com'
                        body={'token':'tid=fixture;proxy-ep='+host+';','expires_at':__import__('time').time()+3600}
                        if mode==8: body['token']='token-without-proxy'
                        if mode==3: status=401
                        if mode==5: body.pop('token')
                    else:
                        assert action=='models' and self.headers['X-GitHub-Api-Version']=='2026-06-01' and self.headers['Authorization'].startswith('Bearer tid=fixture;') if mode!=8 else self.headers['Authorization']=='Bearer token-without-proxy'
                        body={'data':[{'id':'model-a','model_picker_enabled':mode not in (1,2),'policy':{'state':'enabled'}},{'id':'model-b','policy':{'state':'enabled'}},{'id':'excluded','model_picker_enabled':True,'capabilities':{'supports':{'tool_calls':False}}}]}
                        if mode==4: status=500
                        if mode==6: body={'invalid':True}
                    wire=json.dumps(body).encode();self.send_response(status);self.send_header('Content-Length',str(len(wire)));self.end_headers();self.wfile.write(wire)
                def do_POST(self):
                    raw=self.rfile.read(int(self.headers['Content-Length']))
                    if self.path.startswith('/copilot0/'):
                        assert self.headers['Authorization']=='Bearer tid=fixture;proxy-ep=proxy.individual.githubcopilot.com;'
                        route=self.path.rsplit('/',1)[1];copilot_requests.append((0,route));assert 'private-github-token' not in raw.decode()
                        if route=='chat': wire=b'data: {"choices":[{"delta":{},"finish_reason":"stop"}]}\n\ndata: [DONE]\n\n'
                        elif route=='responses': wire=b'data: {"type":"response.completed","response":{"status":"completed"}}\n\n'
                        else: wire=b'data: {"type":"message_start","message":{"id":"copilot-fixture","usage":{"input_tokens":1}}}\n\ndata: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":0}}\n\ndata: {"type":"message_stop"}\n\n'
                        self.send_response(200);self.send_header('Content-Length',str(len(wire)));self.end_headers();self.wfile.write(wire);return
                    if self.path.startswith('/browser'):
                        mode=int(self.path.split('/')[1].removeprefix('browser'));form=parse_qs(raw.decode());browser_requests.append((mode,form));wire=json.dumps({'access_token':'e30.'+payload+'.signature','refresh_token':'rotated-refresh','expires_in':3600}).encode();self.send_response(500 if mode==10 else 200);self.send_header('Content-Length',str(len(wire)));self.end_headers();self.wfile.write(wire);return
                    if self.path.startswith('/device'):
                        prefix,action=self.path.rsplit('/',1);mode=int(prefix.removeprefix('/device'));status=200
                        if action=='model':
                            device_requests.append((mode,action,{}));assert self.headers['chatgpt-account-id']=='pangu-oauth-account' and self.headers['Authorization']=='Bearer e30.'+payload+'.signature'
                            wire=b'data: {"type":"response.completed","response":{"status":"completed"}}\n\n'
                        else:
                            form=json.loads(raw) if action!='exchange' else parse_qs(raw.decode());device_requests.append((mode,action,form))
                            if action=='start': body={'device_auth_id':'private-device','user_code':'ABCD-EFGH','interval':'bad' if mode==5 else '0'}
                            elif action=='poll':
                                count=device_polls.get(mode,0)+1;device_polls[mode]=count
                                if mode==4: status=401;body={'error':'access_denied'}
                                elif mode==3 or (mode==0 and count==4): status=429;body={'error':{'code':'slow_down'}}
                                elif mode==0 and count in (1,2): status=403 if count==1 else 404;body={'pending':True}
                                elif mode==0 and count==3: status=400;body={'error':'deviceauth_authorization_pending'}
                                else: body={'authorization_code':'private-authorization +/&=','code_verifier':'private-verifier +/&='}
                            else:
                                body={'access_token':'invalid-token' if mode==8 else 'e30.'+payload+'.signature','refresh_token':'rotated-refresh','expires_in':3600}
                                if mode==6: status=500
                            wire=json.dumps(body).encode()
                        self.send_response(status);self.send_header('Content-Length',str(len(wire)));self.end_headers()
                        try: self.wfile.write(wire)
                        except (BrokenPipeError,ConnectionResetError): pass
                        return
                    form=parse_qs(raw.decode());requests.append((self.path,form))
                    body={'access_token':'e30.'+payload+'.signature','refresh_token':'rotated-refresh','expires_in':3600}
                    if self.path=='/missing': body.pop('refresh_token')
                    if self.path=='/bad-jwt': body['access_token']='invalid-token'
                    wire=json.dumps(body).encode();self.send_response(401 if self.path=='/denied' else 200);self.send_header('Content-Length',str(len(wire)));self.end_headers();self.wfile.write(wire)
            server=ThreadingHTTPServer(('127.0.0.1',0),OAuthReceiver);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
            try:
                subprocess.run([str(output),'--oauth-refresh',f'http://127.0.0.1:{server.server_port}'],check=True,timeout=30)
                subprocess.run([str(output),'--oauth-device',f'http://127.0.0.1:{server.server_port}'],check=True,timeout=30)
                subprocess.run([str(output),'--oauth-browser',f'http://127.0.0.1:{server.server_port}'],check=True,timeout=30)
                subprocess.run([str(output),'--oauth-copilot',f'http://127.0.0.1:{server.server_port}'],check=True,timeout=30)
            finally: server.shutdown();server.server_close();thread.join()
            assert len(requests)==4 and all(form=={'grant_type':['refresh_token'],'refresh_token':['refresh +/&='],'client_id':['app_EMoamEEZ73f0CkXaXp7hrann']} for _,form in requests),requests
            assert not any(mode==7 for mode,_ in copilot_requests) and all((0,route) in copilot_requests for route in ('chat','responses','anthropic'))
            assert {mode for mode,_ in browser_requests}=={0,1,2,3,6,8,10}
            for mode,form in browser_requests:
                assert form['grant_type']==['authorization_code'] and form['client_id']==['app_EMoamEEZ73f0CkXaXp7hrann'] and form['code']==['private-browser-code'] and len(form['code_verifier'][0])==43 and form['redirect_uri'][0].startswith('http://localhost:')
            source=(ROOT/'references/pi/packages/ai/src/auth/oauth/openai-codex.ts').read_text();source=source[source.index('function parseAuthorizationInput('):source.index('function decodeJwt(')]
            cases=['','  ','code',' code ','code#state','code#state#ignored','code=one+two&state=hello%2Bworld','https://example.com/callback?code=one%2Btwo&state=s','https://example.com/empty','mailto:test','code=first&code=second&state=','code=#state','a#','https://example.com/?code=%E4%B8%AD%E6%96%87']
            oracle="import{readFileSync}from'node:fs';import{stripTypeScriptTypes}from'node:module';const x=JSON.parse(readFileSync(0,'utf8'));const f=new Function(stripTypeScriptTypes(x.source,{mode:'strip'})+';return parseAuthorizationInput')();process.stdout.write(JSON.stringify(x.cases.map(f)));"
            expected=json.loads(subprocess.check_output(['node','--input-type=module','-e',oracle],input=json.dumps({'source':source,'cases':cases}),text=True))
            for case,wanted in zip(cases,expected): assert json.loads(subprocess.check_output([str(output),'--parse-auth',case],text=True))==wanted,(case,wanted)
            assert device_polls[0]==5 and any(mode==0 and action=='model' for mode,action,_ in device_requests)
            for mode,action,form in device_requests:
                if action=='start': assert form=={'client_id':'app_EMoamEEZ73f0CkXaXp7hrann'}
                elif action=='poll': assert form=={'device_auth_id':'private-device','user_code':'ABCD-EFGH'}
                elif action=='exchange': assert form=={'grant_type':['authorization_code'],'client_id':['app_EMoamEEZ73f0CkXaXp7hrann'],'code':['private-authorization +/&='],'code_verifier':['private-verifier +/&='],'redirect_uri':['https://login.invalid/callback']}

        if not any((args.live_cost,args.live,args.live_agent,args.live_conversations,args.live_process_conversations,args.live_extensions,args.live_compaction,args.live_auto_compaction,args.live_background_compaction,args.live_dynamic_context)):
            for api,base,options in (
                ('google-generative-ai',None,'{}'),
                ('google-generative-ai','https://proxy.invalid/v1beta','{}'),
                ('google-vertex',None,'{"authMode":"api-key"}'),
                ('google-vertex',None,'{"project":"pangu","location":"global"}'),
                ('azure-openai-responses',None,'{"azureResourceName":"pangu","azureDeploymentName":"pangu-deployment"}'),
                ('bedrock-converse-stream',None,'{"region":"us-west-2"}'),
            ):
                env=os.environ.copy()
                for key in ('PANGU_MODEL_ENDPOINT','PANGU_MODEL_BASE_URL'):
                    env.pop(key,None)
                env.update(PANGU_MODEL_API=api,PANGU_MODEL_NAME='gemini-3-flash',PANGU_MODEL_API_KEY='configuration-placeholder',PANGU_MODEL_OPTIONS=options)
                if base: env['PANGU_MODEL_BASE_URL']=base
                subprocess.run([str(output),'--model-configuration'],env=env,check=True,timeout=30)
        library=Path(directory)/'tasks.so'
        if args.native_tasks or args.live_extensions:
            subprocess.run(['/usr/bin/clang-23','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-fPIC','-shared',*sanitize,'-DPG_TASK_FIXTURE',str(ROOT/'tests/runtime/product_plugin_fixture.c'),*flags,'-o',str(library)],check=True)
        if args.live_process_conversations:
            for suffix in ('','-cancel'):
                workspace=Path(directory)/('workspace'+suffix)
                workspace.mkdir()
                for stage in ('seed','resume'):
                    subprocess.run([str(output),'--nested-process-'+stage+suffix,str(workspace)],check=True,timeout=120)
            return
        subprocess.run([str(output),*(['--live-cost'] if args.live_cost else ['--live-extensions',str(library)] if args.live_extensions else ['--live-agent'] if args.live_agent else ['--live-conversations'] if args.live_conversations else ['--native-tasks',str(library)] if args.native_tasks else ['--compaction-hooks'] if args.compaction_hooks else ['--live-dynamic-context'] if args.live_dynamic_context else ['--live-background-compaction'] if args.live_background_compaction else ['--live-auto-compaction'] if args.live_auto_compaction else ['--live-compaction'] if args.live_compaction else ['--live'] if args.live else [])],check=True,timeout=330)
if __name__=='__main__':
    main()
