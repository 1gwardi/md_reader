from pathlib import Path
import subprocess
import select
import sys
import tempfile

binary, harness = sys.argv[1:]
with tempfile.TemporaryDirectory() as directory:
    big = Path(directory) / 'big.md'
    with big.open('wb') as stream: stream.truncate(17*1024*1024)
    server = subprocess.Popen([binary,'--no-window',directory], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        ready,_,_ = select.select([server.stdout],[],[],5)
        if not ready: raise RuntimeError('Server did not start')
        url = server.stdout.readline().strip()
        if not url.startswith('http://'): raise RuntimeError(server.stderr.read())
        subprocess.run([harness,url+'files/big.md',str(big),'review',str(Path(__file__).with_name('review_checks.js'))],check=True,timeout=30)
    finally:
        server.terminate(); server.communicate(timeout=5)
