"""Run the real native window and inspect its WebKit-rendered document."""
from pathlib import Path
import select
import subprocess
import sys
import tempfile

binary, harness = sys.argv[1:]
with tempfile.TemporaryDirectory() as directory:
    document = Path(directory) / 'native window.md'
    document.write_text('# Native window test\n\n' + 'Paragraph for scrolling.\n\n' * 100)
    server = subprocess.Popen([binary, '--no-window', str(document)], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    try:
        ready, _, _ = select.select([server.stdout], [], [], 5)
        if not ready:
            raise RuntimeError('Server did not start')
        url = server.stdout.readline().strip()
        if not url.startswith('http://127.0.0.1:'):
            raise RuntimeError(server.stderr.read())
        subprocess.run([harness, url, str(document)], check=True, timeout=25)
    finally:
        server.terminate()
        server.communicate(timeout=5)

    chapters = Path(directory) / 'chapters'
    chapters.mkdir()
    nested = chapters / 'native window.md'
    nested.write_text('# Native window test\n\n' + 'Paragraph for scrolling.\n\n' * 100)
    (chapters / 'second.md').write_text('# Second document\n')
    server = subprocess.Popen([binary, '--no-window', directory], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    try:
        ready, _, _ = select.select([server.stdout], [], [], 5)
        if not ready:
            raise RuntimeError('Directory server did not start')
        url = server.stdout.readline().strip()
        if not url.startswith('http://127.0.0.1:'):
            raise RuntimeError(server.stderr.read())
        subprocess.run([harness, url, str(nested), 'directory'], check=True, timeout=25)
    finally:
        server.terminate()
        server.communicate(timeout=5)

    # Exercise the CLI's GUI + server lifecycle together, including Ctrl+C.
    import signal
    app = subprocess.Popen([binary, str(document)], stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, text=True)
    try:
        ready, _, _ = select.select([app.stdout], [], [], 5)
        if not ready or not app.stdout.readline().startswith('http://127.0.0.1:'):
            raise RuntimeError('GUI CLI did not start')
        import time
        time.sleep(1)
        if app.poll() is not None:
            raise RuntimeError('GUI CLI exited unexpectedly')
        app.send_signal(signal.SIGINT)
        stdout, stderr = app.communicate(timeout=5)
        if app.returncode != 0:
            raise RuntimeError(f'GUI CLI failed to stop: {stderr}')
    finally:
        if app.poll() is None:
            app.kill()
            app.communicate()
