#!/usr/bin/env python3
"""Run OUTSIDE the socket-denied sandbox. Public GETs only; no keys or redirects.
Raw provider bodies and provenance are test inputs, never bundled in apps.
Uses system CA roots; this is NOT proof of the ESP32 TLS stack.
"""
import datetime, hashlib, json, pathlib, ssl, sys, urllib.request, urllib.error
out = pathlib.Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=False)  # never overwrite another writer's evidence
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args):
        return None
opener = urllib.request.build_opener(NoRedirect(), urllib.request.HTTPSHandler(context=ssl.create_default_context()))
records=[]
def capture(name, url, limit=8192):
    record={'name':name,'url':url,'fetched_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'ca_paths':ssl.get_default_verify_paths()._asdict()}
    try:
        request=urllib.request.Request(url, headers={'Accept':'text/plain' if name=='article' else 'application/json'})
        try:
            response=opener.open(request, timeout=30)
        except urllib.error.HTTPError as error:
            response=error
        with response:
            body=response.read(4*1024*1024+1)
            if len(body)>4*1024*1024:
                raise ValueError('fixture exceeds capture ceiling; not stored')
            (out/(name+'.body')).write_bytes(body)
            (out/(name+'.headers')).write_text(str(response.headers))
            record.update(status=response.status, bytes=len(body),sha256=hashlib.sha256(body).hexdigest(),
                          within_body_limit=len(body)<=limit,content_encoding=response.headers.get('Content-Encoding'),
                          framing=response.headers.get('Transfer-Encoding') or response.headers.get('Content-Length') or 'close-delimited')
            record['candidate_for_v3']=(response.status==200 and len(body)<=limit and
                record['content_encoding'] in (None,'identity') and record['framing']!='close-delimited')
            return body
    except Exception as error:
        record['error']=str(error)
    finally:
        records.append(record)
        (out/'provenance.json').write_text(json.dumps(records,indent=2)+'\n')
feed=capture('feed','https://hacker-news.firebaseio.com/v0/topstories.json')
if feed:
    ids=json.loads(feed)
    if isinstance(ids,list) and ids and type(ids[0]) is int and 0<ids[0]<=2147483647:
        capture('item','https://hacker-news.firebaseio.com/v0/item/'+str(ids[0])+'.json')
capture('article','https://r.jina.ai/https://example.com/')
capture('trivia','https://opentdb.com/api.php?amount=1&type=multiple&encode=url3986')
capture('connections','https://raw.githubusercontent.com/Eyefyre/NYT-Connections-Answers/main/connections.json')
capture('xkcd','https://xkcd.com/info.0.json')
print(json.dumps(records,indent=2))
if any('error' in r for r in records):
    sys.exit(1)
