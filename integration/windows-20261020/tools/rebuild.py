"""Rebuild one pinned owner profile from the packaged source and require its exact hash."""
import argparse,json
from evaluate import ROOT,build,write

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--profile',choices=['off','a2','a4','combined'],required=True)
    parser.add_argument('--name',required=True,help='New immutable build ID')
    args=parser.parse_args()
    settings=dict(A2_BUSH_FAST=0,A2_FAST_AMBUSH=0,A2_CMICRO_FAST=0,A2_GOV=0,A2_SNAP=0,A2_TD_AUTH=0,
                  A2_FLEE_HIT=0,A2_TEL=0,A2_CHECK=0,NOVA_V36_DRAFT=0,NOVA_MAPID_STRICT=0)
    if args.profile in ['a2','combined']:
        settings.update(A2_BUSH_FAST=1,A2_FAST_AMBUSH=1,A2_CMICRO_FAST=1,A2_GOV=1,A2_SNAP=4,A2_TD_AUTH=1,A2_FLEE_HIT=1)
    if args.profile in ['a4','combined']:settings['NOVA_V36_DRAFT']=1
    record=build(args.name,settings,source='source',normalize=True)
    key={'off':'integrated-off-v2','a2':'integrated-a2-v2','a4':'integrated-a4-v2','combined':'integrated-a2-a4-v2'}[args.profile]
    expected=json.loads((ROOT/'OWNER_BUILDS.json').read_text())[key]['sha256']
    assert record['artifact_sha256']==expected, 'Rebuilt artifact differs from recorded profile'
    print('Exact profile reproduced:',args.profile,expected)

if __name__=='__main__':main()
