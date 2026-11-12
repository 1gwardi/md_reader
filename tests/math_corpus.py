"""Render existing Markdown documents in WKWebView without changing the files."""
from pathlib import Path
import select
import subprocess
import sys

binary, harness, *documents = sys.argv[1:]
if not documents:
    sys.exit('Pass at least one Markdown document')
failed = False
for filename in documents:
    document = Path(filename).resolve(strict=True)
    server = subprocess.Popen([binary, '--no-window', str(document)], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    try:
        ready, _, _ = select.select([server.stdout], [], [], 5)
        if not ready:
            raise RuntimeError('Server did not start')
        url = server.stdout.readline().strip()
        if not url.startswith('http://127.0.0.1:'):
            raise RuntimeError(server.stderr.read())
        result = subprocess.run([harness, url, str(document), 'corpus'], timeout=25)
        failed |= result.returncode != 0
    finally:
        server.terminate()
        server.communicate(timeout=5)
sys.exit(1 if failed else 0)
