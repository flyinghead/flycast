#!/usr/bin/env python3
"""Native renderer CLI restart matrix; no app, windows, games or system-cache deletion."""
from pathlib import Path
import argparse,json,os,re,statistics,subprocess,tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary',type=Path)
parser.add_argument('--trials',type=int,default=2)
parser.add_argument('--api-validation',action='store_true')
args=parser.parse_args()
if not 1<=args.trials<=4:parser.error('trials must be 1..4')
work=Path('/private/tmp/flycast-metal4-todo');work.mkdir(exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith('MTL_SHADER_VALIDATION')}
env.update(MTL_DEBUG_LAYER=str(int(args.api_validation)),MTL_SHADER_VALIDATION='0')
runs=[]
with tempfile.TemporaryDirectory(prefix='cache-transitions-',dir=work) as directory:
    root=Path(directory)
    for trial in range(args.trials):
        for policy in (['baseline','optimized'] if trial%2==0 else ['optimized','baseline']):
            cache=root/f'{trial}-{policy}';cache.mkdir()
            for phase in ['cold','warm']:
                log=work/f'cache-transition-{int(args.api_validation)}-{trial}-{policy}-{phase}.log'
                command=[str(args.binary.resolve()),'--cache-profile',str(cache),policy,phase]
                with log.open('w') as output:result=subprocess.run(command,env=env,stdout=output,stderr=subprocess.STDOUT)
                text=log.read_text()
                if result.returncode or re.search(r'^FAIL|error:',text,re.M):raise RuntimeError(f'Child failed: {log} exit={result.returncode}')
                assert text.rstrip().endswith('PASS actual offscreen renderer: no app/window/game, isolated temporary cache')
                rows=[dict(re.findall(r'(\w+)=([^\s]+)',s)) for s in text.splitlines() if s.startswith('Metal cache frame ')]
                assert len(rows)==384 and text.count('192 exact full-frame CPU-oracle images')==2
                summary=json.loads(next(s.removeprefix('Metal cache summary ') for s in text.splitlines() if s.startswith('Metal cache summary ')))
                assert summary['pending']==0 and not summary['accepting'] and summary['timing']
                assert summary['copies']==summary['queued']==summary['recordings']
                if phase=='cold':assert summary['misses']>0 and summary['recordings']>0
                else:assert summary['hits']>0
                samples={}
                for mode in ['standard','OIT']:
                    group=[r for r in rows if r['mode']==mode]
                    for label,selected in [('all',group),('ordinary',[r for r in group if r['restart']=='0']),('restart',[r for r in group if r['restart']=='1'])]:
                        values=sorted(float(r['cpu_us']) for r in selected)
                        samples[mode+'_'+label]={'count':len(values),'median_us':statistics.median(values),'p95_us':values[int((len(values)-1)*.95)],'p99_us':values[int((len(values)-1)*.99)],'max_us':max(values)}
                runs.append({'trial':trial,'policy':policy,'phase':phase,'log':str(log),'summary':summary,'frame_summaries':samples,'frames':rows})
                print(json.dumps({'trial':trial,'policy':policy,'phase':phase,'hits':summary['hits'],'misses':summary['misses'],'writer':summary['recordings'],'duplicates':summary['duplicates'],'copies':summary['copies'],'hinted':summary['hinted']}),flush=True)
output=work/f'cache-transition-result-{int(args.api_validation)}.json'
output.write_text(json.dumps({'api_validation':args.api_validation,'shader_validation':False,'system_cache_reset':False,'fixture_cache_removed':True,'runs':runs},separators=(',',':'))+'\n')
print('PASS actual cache restart matrix:',len(runs),'processes;',len(runs)*384,'exact full images; result',output,flush=True)
