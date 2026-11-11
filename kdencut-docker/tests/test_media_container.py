"""Check the production image starts its media service behind the editor login."""
import base64
import json
import subprocess
import time
import urllib.error
import urllib.request
import uuid

url = 'http://127.0.0.1:3306/kdencut-media/'
auth = 'Basic ' + base64.b64encode(b'ci:media-test').decode()


def call(path, data=None, authenticated=True, method=None):
    headers = {'X-KdenCut-Media': '1'}
    if authenticated: headers['Authorization'] = auth
    return urllib.request.urlopen(urllib.request.Request(url+path, data=data, headers=headers, method=method), timeout=5)


try:
    subprocess.run(['docker', 'run', '-d', '--name', 'kdencut-media-smoke', '--shm-size=1g',
                    '-e', 'CUSTOM_USER=ci', '-e', 'PASSWORD=media-test',
                    '-p', '127.0.0.1:3306:3000', 'kdencut-ci'], check=True)
    for attempt in range(90):
        try:
            with call('status') as response: json.load(response)
            break
        except (OSError, urllib.error.URLError):
            if attempt == 89: raise
            time.sleep(1)
    try:
        call('status', authenticated=False)
        raise AssertionError('Media endpoint bypasses editor login')
    except urllib.error.HTTPError as error:
        assert error.code == 401, error.code
    request_id = str(uuid.uuid4())
    request = json.dumps({'id':request_id, 'kind':'media', 'created':time.time()*1000})
    subprocess.run(['docker','exec','-i','-u','abc','kdencut-media-smoke','python3','-c',
                    'import os,sys,pathlib; pathlib.Path(os.environ["KDENCUT_MEDIA_BRIDGE"],"request.json").write_text(sys.stdin.read())'],
                   input=request, text=True, check=True)
    with call('upload?request='+request_id+'&name=smoke.mov', b'container upload bytes', method='PUT') as response:
        token = json.load(response)['token']
    with call('finish', json.dumps({'request':request_id,'files':[token]}).encode(), method='POST') as response:
        json.load(response)
    subprocess.run(['docker','exec','kdencut-media-smoke','python3','-c',
                    'import json,pathlib,os,sys; result=json.loads(pathlib.Path(os.environ["KDENCUT_MEDIA_BRIDGE"],sys.argv[1]+".json").read_text()); assert pathlib.Path(result["paths"][0]).read_bytes()==b"container upload bytes"', request_id], check=True)
    print('Production service, login, upload, and native handoff passed')
finally:
    subprocess.run(['docker','logs','--tail','30','kdencut-media-smoke'])
    subprocess.run(['docker','rm','-f','kdencut-media-smoke'])
