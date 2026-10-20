"""Compare standard Wasm sections separately from non-executable custom metadata."""
import argparse, hashlib, json, pathlib
from evaluate import ROOT, write

def sections(path):
    data=pathlib.Path(path).read_bytes()
    if data[:8]!=b'\0asm\x01\0\0\0': raise ValueError('Expected Wasm1 module')
    def leb(pos):
        value=shift=0
        while True:
            byte=data[pos];pos+=1;value|=(byte&127)<<shift
            if not byte&128:return value,pos
            shift+=7
            if shift>35:raise ValueError('Invalid section length')
    out=[];pos=8
    while pos<len(data):
        kind=data[pos];size,start=leb(pos+1);end=start+size
        if end>len(data):raise ValueError('Truncated section')
        payload=data[start:end];name=None
        if kind==0:
            length,nstart=leb(start);name=data[nstart:nstart+length].decode('utf-8')
        out.append(dict(kind=kind,custom_name=name,length=size,sha256=hashlib.sha256(payload).hexdigest()))
        pos=end
    return out

def compare(a,b,out):
    aa=sections(a);bb=sections(b)
    standard=lambda rows:[r for r in rows if r['kind']!=0]
    result=dict(a=str(a),b=str(b),all_standard_sections_equal=standard(aa)==standard(bb),
        sections_a=aa,sections_b=bb,
        interpretation='Standard Wasm sections include executable code, imports, data and initialization. Custom sections are non-executable metadata. This comparison does not replace SDK legality or engine-match checks.')
    write(out,result);print(json.dumps(dict(all_standard_sections_equal=result['all_standard_sections_equal'],out=str(out))),flush=True)
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('a');p.add_argument('b');p.add_argument('--out',required=True);args=p.parse_args();compare(args.a,args.b,args.out)
