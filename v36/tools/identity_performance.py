"""Registered corrected identity/rows gates. Old incorrect-header builds excluded."""
from evaluate import ROOT,PROJECT,freeze,execute,write,manifest

def run(sid,candidate,opponents,maps,purpose):
    if not (ROOT/'schedules'/(sid+'.json')).exists():freeze(sid,candidate,opponents,maps,purpose)
    return execute(sid,3)
def main():
    official=sorted((PROJECT/'maps').glob('*.map26a'));maps=sorted((ROOT/'maps/screen').glob('*.map26a'))
    write(ROOT/'EXPERIMENTS_IDENTITY_ROWS.json',dict(header_sha256='3e84b617d5d38f92260ccbb6ee46e8c0937260abc75b461ad97cd3c87edeccf0',
        gate='Corrected native20scenarios170draftpicks210physicalmodules allassertionspassed. Earlier editormixedconstants rejected and not promoted.',
        rows_development='2wins+132submittedgem mean onone fixedrowspair, noindependence/CIclaim. Requires unchangedoffrowsbehavior and largerperformancegates.',
        fixed_screens='Strictsame-sourceOFF androwsstrict-onlycontrol, each17officialmaps34games+200syntheticmaps400games. No adaptivepromotionmidchunk.',
        acceptance='Zero nativefaults, mechanism collectiondelay confirmed, no material outside-rows regression; thenfreshvalidation of stack. No performanceproof fromsinglemap.'))
    for name,control in [('strict-fixed','strict-fixed-off'),('rows-intent-fixed','strict-fixed')]:
        if manifest(name)['source_tree']['sha256']!=manifest(control)['source_tree']['sha256']:raise ValueError('Same-source control required')
        run(name+'-official',name,[control],official,'Corrected identity / rows isolated17map34game gate')
        run(name+'-screen',name,[control],maps,'Corrected identity / rows fixed200newsyntheticmaps400games; unknown fallback and fuel checks')
    run('corrected-off-parity','strict-fixed-off',['baseline'],official,'AllnewpoliciesOFF aftercorrectedidentitydata andE06source: rawreplayparity prerequisite')
    pool=['spar_fish','spar_mite','spar_bush','live_platform_v35']
    run('rows-local-pool','rows-intent-fixed',pool,official,'Corrected rows+identity fixedlocal4opponentpool136games, matchedbaseline')

if __name__=='__main__':main()
