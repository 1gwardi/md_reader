"""Verify pinned local vendor files; this does not query security advisories."""
import hashlib
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
manifest = root / 'third_party/SHA256SUMS'
failed = []
count = 0
for line in manifest.read_text().splitlines():
    digest, name = line.split('  ', 1)
    path = (root / name).resolve()
    if not path.is_relative_to(root / 'third_party') or not path.is_file():
        failed.append(name)
    elif hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        failed.append(name)
    count += 1
if failed:
    sys.exit('Vendored file mismatch: ' + ', '.join(failed))
print(f'Verified {count} vendored files')
