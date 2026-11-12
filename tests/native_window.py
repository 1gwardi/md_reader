"""Run the real native window and inspect its WebKit-rendered document."""
from pathlib import Path
import select
import subprocess
import sys
import tempfile

binary, harness = sys.argv[1:]
math_fixture = r'''
Inline $x_0 + y_0$.

Legacy indices: $d\_{\text{standoff}}$ and $\text{view\_max}\_{\text{color}}$.

$$
\begin{aligned}
d_{\text{fwd}} &= X_{\text{rover}} - X_{\text{start}} \\
d_{\text{left}} &= Y_{\text{rover}} - Y_{\text{start}} \\
X_{\text{ned}} &= x_0 + d_{\text{fwd}} \cdot \cos(yaw_0) + d_{\text{left}} \cdot \sin(yaw_0) \\
Y_{\text{ned}} &= y_0 + d_{\text{fwd}} \cdot \sin(yaw_0) - d_{\text{left}} \cdot \cos(yaw_0)
\end{aligned}
$$

Invalid expression: $\frac{1}{$.

'''
html_fixture = '''
<details id="html-details"><summary>HTML details</summary><p>Hidden details body</p></details>

<h2 id="custom-anchor">Custom HTML anchor</h2>

<p id="html-styled" style="color: rgb(12, 34, 56)">Styled HTML</p>

<img id="html-image" src="pixel.svg" width="32" height="24" onerror="window.htmlExecuted=true">

<script>window.htmlExecuted=true</script>
<iframe srcdoc="<script>parent.htmlExecuted=true</script>"></iframe>
<a id="unsafe-link" href="javascript:window.htmlExecuted=true">Unsafe link</a>
<style>header { display:none }</style>

```html
<em>HTML code example</em>
```
'''
with tempfile.TemporaryDirectory() as directory:
    document = Path(directory) / 'native window.md'
    document.write_text('# Native window test\n\n' + math_fixture + html_fixture + '\n\n' + 'Paragraph for scrolling.\n\n' * 100)
    (Path(directory) / 'pixel.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="32" height="24"><rect width="32" height="24"/></svg>')
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
    nested.write_text('# Native window test\n\n' + math_fixture + html_fixture + '\n\n' + 'Paragraph for scrolling.\n\n' * 100)
    (chapters / 'pixel.svg').write_text((Path(directory) / 'pixel.svg').read_text())
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
