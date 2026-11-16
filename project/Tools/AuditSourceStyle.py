# 外部ライブラリを除いたソースから、波括弧と説明の不足を検出する。
"""Audit authored C++/HLSL; third-party sources are intentionally excluded."""
from pathlib import Path
import json
from tree_sitter import Language,Parser
import tree_sitter_cpp
ROOT=Path(__file__).resolve().parents[2]
PARSER=Parser(Language(tree_sitter_cpp.language()))
EXTENSIONS={'.cpp','.h','.hpp','.hlsl','.hlsli'}

def sources():
    return sorted(p for p in (ROOT/'project').rglob('*') if p.suffix in EXTENSIONS
                  and not {'externals','third_party','generated','Compiled'}.intersection(p.parts))

def walk(node):
    yield node
    for child in node.named_children:
        yield from walk(child)

# ソースの構文木を走査し、波括弧と関数コメントの不足を記録する。
def inspect(path):
    data=path.read_bytes();tree=PARSER.parse(data);missing=[];functions=[];errors=[]
    for node in walk(tree.root_node):
        if node.type=='ERROR' or node.is_missing:
            errors.append(node.start_point.row+1)
        fields=()
        if node.type=='if_statement': fields=('consequence','alternative')
        elif node.type in ('for_statement','for_range_loop','while_statement','do_statement'): fields=('body',)
        for field in fields:
            body=node.child_by_field_name(field)
            if body and body.type=='else_clause':
                body=next((c for c in body.named_children if c.type!='comment'),None)
            if body and body.type not in ('compound_statement','if_statement','preproc_if','preproc_ifdef'):
                missing.append({'line':body.start_point.row+1,'kind':node.type,'field':field})
        if node.type=='function_definition':
            body=node.child_by_field_name('body')
            if body:
                header=data[node.start_byte:body.start_byte].decode('utf-8-sig','replace').strip()
                previous=node.prev_named_sibling
                commented=bool(previous and previous.type=='comment' and node.start_point.row-previous.end_point.row<=2)
                # テンプレートやHLSL属性の手前にあるコメントも、同じ関数の説明として扱う。
                if node.start_point.row>0:
                    prior_line=data.splitlines()[node.start_point.row-1].strip()
                    commented=commented or prior_line.startswith(b'//') or prior_line.endswith(b'*/')
                functions.append({'line':node.start_point.row+1,'name':header,'commented':commented,
                                  'lines':node.end_point.row-node.start_point.row+1})
    return {'path':str(path.relative_to(ROOT)),'missing':missing,'parse_errors':errors,'functions':functions}

if __name__=='__main__':
    import argparse
    ap=argparse.ArgumentParser();ap.add_argument('--report',required=True);ap.add_argument('--check',action='store_true');args=ap.parse_args()
    result=[inspect(p) for p in sources()]
    target=Path(args.report);target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'files':len(result),'unbraced':sum(len(r['missing']) for r in result),
                     'functions':sum(len(r['functions']) for r in result),
                     'uncommented_nontrivial':sum(not f['commented'] and f['lines']>4 for r in result for f in r['functions'])}))
    # 継続チェックでは不足を終了コードへ反映し、将来の省略を検出できるようにする。
    if args.check and any(r['missing'] for r in result):
        raise SystemExit(1)
