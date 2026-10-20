"""Resumable collector for public match groups (drafts, results, end-of-game statistics).

Usage: python collect.py [--hours 48] [--top 150] [--out ../data/groups]

Walks the public global feed (/match-groups, newest first, cursor `before`) back --hours, and saves the detail of
every finished group in which at least one side is ranked in the ladder's top --top at collection time. Existing
files are skipped (dedup by group UUID), so reruns resume. Writes ladder snapshot and a provenance log beside it.
Public data carries no submission IDs/versions (null), so per-version attribution needs the team's own login data.
"""
import argparse
import concurrent.futures as cf
import datetime as dt
import json
import os
import time
import urllib.parse
import urllib.request

API = 'https://2026a.battlecode.co.uk/api/v1'


def get(path):
    err = None
    for attempt in range(4):
        try:
            with urllib.request.urlopen(f'{API}/{path}', timeout=30) as r:
                return json.load(r)
        except Exception as e:
            err = e
            time.sleep(2 + 4 * attempt)
    raise err


def fetch(item):
    gid, dst = item
    d = get(f'match-groups/{gid}')
    tmp = dst + '.part'
    json.dump(d, open(tmp, 'w'))
    os.replace(tmp, dst)
    time.sleep(0.1)
    return 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--hours', type=float, default=24)
    ap.add_argument('--top', type=int, default=150)
    ap.add_argument('--out', default=os.path.join(os.path.dirname(__file__), '..', 'data', 'groups'))
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    root = os.path.dirname(out)

    teams, page = [], 1
    while True:
        d = get(f'ladder?page={page}')
        teams += d['teams']
        if page >= d['pageCount'] or not d['teams']:
            break
        page += 1
    stamp = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    json.dump(teams, open(os.path.join(root, f'ladder_{stamp}.json'), 'w'))
    top = {t['teamId'] for t in teams if t.get('rank') and t['rank'] <= a.top}

    stop = dt.datetime.now(dt.timezone.utc) - dt.timedelta(hours=a.hours)
    cursor, seen, saved, skipped = None, 0, 0, 0
    log = open(os.path.join(root, 'collect_log.jsonl'), 'a')
    pool = cf.ThreadPoolExecutor(4)
    while True:
        q = 'match-groups' + (f'?before={urllib.parse.quote(cursor)}' if cursor else '')
        r = get(q)
        oldest = None
        todo = []
        for g in r['groups']:
            seen += 1
            oldest = g['createdAt']
            if g['status'] != 'finished' or not any(s['teamId'] in top for s in g['sides']):
                skipped += 1
                continue
            dst = os.path.join(out, g['id'] + '.json')
            if not os.path.exists(dst):
                todo.append((g['id'], dst))
        saved += sum(pool.map(fetch, todo))
        if not r.get('more') or not r.get('next') or oldest is None:
            break
        if dt.datetime.fromisoformat(oldest.replace('Z', '+00:00')) < stop:
            break
        cursor = r['next']
        if seen % 500 < 25:
            print(f'{oldest} seen {seen} saved {saved}', flush=True)
    log.write(json.dumps({'at': stamp, 'hours': a.hours, 'top': a.top, 'seen': seen, 'saved': saved,
                          'skipped': skipped, 'oldest': oldest}) + '\n')
    print(f'done: seen {seen}, saved {saved}, skipped {skipped}, oldest {oldest}')


if __name__ == '__main__':
    main()
