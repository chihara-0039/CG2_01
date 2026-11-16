# 変更前を退避し、処理のトークンを照合しながら書式と波括弧をそろえる。
"""Back up and format authored sources; reject any change beyond braces/whitespace."""
from pathlib import Path
from datetime import datetime
import subprocess,shutil,re,json,hashlib
from AuditSourceStyle import ROOT,sources

# Strings remain intact so changes inside literals cannot pass the token check.
TOKEN=re.compile(rb'R"([^ ()\\\t\r\n]{0,16})\(.*?\)\1"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\r\n]*|/\*.*?\*/|[^\s{}]',re.S)
def tokens(data):
    return [m.group() for m in TOKEN.finditer(data) if not m.group().startswith((b'//',b'/*'))]

if __name__=='__main__':
    snapshot=ROOT/'generated/style-review'/('snapshot-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    formatter=ROOT/'.venv/Scripts/clang-format.exe'
    result=[]
    for path in sources():
        before=path.read_bytes()
        after=subprocess.run([str(formatter),'--style=file','--assume-filename='+str(path)],input=before,capture_output=True,check=True).stdout
        if tokens(before)!=tokens(after):
            raise RuntimeError('Non-formatting change rejected: '+str(path))
        if after!=before:
            backup=snapshot/path.relative_to(ROOT);backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,backup)
            path.write_bytes(after)
            result.append({'file':str(path.relative_to(ROOT)),'before_sha256':hashlib.sha256(before).hexdigest(),
                           'added_brace_pairs':(after.count(b'{')-before.count(b'{'))})
    snapshot.mkdir(parents=True,exist_ok=True)
    (snapshot/'changes.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'changed_files':len(result),'brace_pairs':sum(r['added_brace_pairs'] for r in result),'snapshot':str(snapshot)},ensure_ascii=False))
