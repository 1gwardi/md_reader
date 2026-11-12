"""Exercise the actual CLI and HTTP server, without opening a window."""
import http.client
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import tempfile
import unittest
from urllib.parse import quote, urlsplit

BINARY = str(Path(sys.argv.pop(1)).resolve())


class ServerCase(unittest.TestCase):
    directory_mode = False

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.doc = self.root / "тест с пробелами.md"
        self.doc.write_text(
            '# Привет\n\n**Жирный** и [ссылка](https://example.com)\n\n'
            '```cpp\nint x = 1;\n```\n\n'
            '| A | B |\n|---|---|\n| 1 | 2 |\n\n- [x] Готово\n\n'
            '<script>alert(1)</script>\n', encoding="utf-8")
        self.process = subprocess.Popen(
            [BINARY, "--no-window", str(self.root if self.directory_mode else self.doc)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(self.stop)
        ready, _, _ = select.select([self.process.stdout], [], [], 5)
        self.assertTrue(ready, "Server did not print its URL")
        url = self.process.stdout.readline().strip()
        self.assertTrue(url.startswith("http://127.0.0.1:"), self.process.stderr.read() if self.process.poll() is not None else url)
        self.address = urlsplit(url)

    def stop(self):
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGTERM)
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.process.stdout.close()
        self.process.stderr.close()

    def request(self, path="/", headers=None, method="GET"):
        connection = http.client.HTTPConnection(self.address.hostname, self.address.port, timeout=3)
        try:
            connection.request(method, path, headers=headers or {})
            response = connection.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            connection.close()

    def content(self, etag=None):
        return self.request("/__mdread/content", {"If-None-Match": etag} if etag else {})


class ReaderTest(ServerCase):
    def test_math_source_and_bundled_assets(self):
        self.doc.write_text(r'''Inline $x_0 + y_0$.

$$
\begin{aligned}
d_{\text{fwd}} &= X_{\text{rover}} - X_{\text{start}} \\
d_{\text{left}} &= Y_{\text{rover}} - Y_{\text{start}} \\
X_{\text{ned}} &= x_0 + d_{\text{fwd}} \cdot \cos(yaw_0) + d_{\text{left}} \cdot \sin(yaw_0) \\
Y_{\text{ned}} &= y_0 + d_{\text{fwd}} \cdot \sin(yaw_0) - d_{\text{left}} \cdot \cos(yaw_0)
\end{aligned}
$$

```tex
$$not_math$$
```

Escaped price: \$100.
''')
        status, _, body = self.content()
        self.assertEqual(status, 200)
        html = json.loads(body)['html']
        self.assertIn('<x-equation>x_0 + y_0</x-equation>', html)
        self.assertIn('<x-equation type="display">', html)
        self.assertIn(r'\begin{aligned}', html)
        self.assertIn('\\\\', html)
        self.assertEqual(html.count('<x-equation'), 2)
        self.assertIn('$$not_math$$', html)
        self.assertIn('Escaped price: $100.', html)
        _, headers, page = self.request()
        self.assertIn("font-src 'self'", headers['Content-Security-Policy'])
        self.assertIn(b'/__mdread/katex/katex.min.js', page)
        status, headers, css = self.request('/__mdread/katex/katex.min.css')
        self.assertEqual(status, 200)
        self.assertTrue(headers['Content-Type'].startswith('text/css'))
        import re
        fonts = set(re.findall(rb'url\((fonts/[^)]+)\)', css))
        self.assertEqual(len(fonts), 20)
        for font in fonts:
            status, headers, data = self.request('/__mdread/katex/' + font.decode())
            self.assertEqual(status, 200, font)
            self.assertEqual(headers['Content-Type'], 'font/woff2')
            self.assertTrue(data.startswith(b'wOF2'))
        self.assertEqual(self.request('/__mdread/katex/../../README.md')[0], 404)
        self.assertEqual(self.request('/__mdread/katex/missing.woff2')[0], 404)

    def test_render_and_page(self):
        status, headers, body = self.request()
        self.assertEqual(status, 200)
        self.assertIn("тест с пробелами.md", body.decode())
        self.assertIn("script-src 'self'", headers["Content-Security-Policy"])
        status, headers, body = self.content()
        html = json.loads(body)["html"]
        for fragment in ('<h1>Привет</h1>', '<strong>Жирный</strong>', '<table>', 'type="checkbox"', 'class="language-cpp"'):
            self.assertIn(fragment, html)
        # The API carries parsed HTML; the web view sanitizes it before insertion.
        self.assertIn('<script>', html)
        self.assertNotIn('<script>alert(1)</script>', self.request()[2].decode())
        self.assertEqual(self.content(headers['ETag'])[0], 304)
        for path, mime in (("style.css", "text/css"), ("app.js", "text/javascript"), ("purify.js", "text/javascript")):
            status, headers, body = self.request("/__mdread/" + path)
            self.assertEqual(status, 200)
            self.assertTrue(headers["Content-Type"].startswith(mime))
            self.assertTrue(body)

    def test_embedded_html_and_html_code_examples(self):
        self.doc.write_text('''# HTML support

Inline <strong>bold HTML</strong>, H<sub>2</sub>O and x<sup>2</sup>.<br>New line.

<details open>
<summary>Show details</summary>
<p style="color: rgb(180, 30, 30)">HTML paragraph</p>
<table><tr><th>Key</th><td>Value</td></tr></table>
</details>

<h2 id="custom-anchor">Custom anchor</h2>

```html
<strong>This is source code</strong>
```
''', encoding='utf-8')
        status, _, body = self.content()
        self.assertEqual(status, 200)
        html = json.loads(body)['html']
        for fragment in ('<strong>bold HTML</strong>', '<sub>2</sub>', '<sup>2</sup>',
                         '<details open>', '<summary>Show details</summary>',
                         'style="color: rgb(180, 30, 30)"', '<h2 id="custom-anchor">', '<th>Key</th>'):
            self.assertIn(fragment, html)
        self.assertIn('&lt;strong&gt;This is source code&lt;/strong&gt;', html)
        status, headers, body = self.request()
        self.assertEqual(status, 200)
        self.assertIn("style-src 'self' 'unsafe-inline'", headers['Content-Security-Policy'])
        self.assertNotIn('<details', body.decode())
        self.assertLess(body.index(b'/__mdread/purify.js'), body.index(b'/__mdread/app.js'))

    def test_changes_atomic_save_and_recovery(self):
        _, headers, _ = self.content()
        etag = headers['ETag']
        stat = self.doc.stat()
        self.doc.write_text("# Новая версия\n", encoding="utf-8")
        os.utime(self.doc, ns=(stat.st_atime_ns, stat.st_mtime_ns))
        status, headers, body = self.content(etag)
        self.assertEqual(status, 200)
        self.assertIn('Новая версия', json.loads(body)['html'])
        etag = headers['ETag']
        replacement = self.root / "save.tmp"
        replacement.write_text("# Atomic save\n")
        replacement.replace(self.doc)
        status, headers, body = self.content(etag)
        self.assertEqual(status, 200)
        self.assertIn('Atomic save', json.loads(body)['html'])
        etag = headers['ETag']
        self.doc.unlink()
        self.assertEqual(self.content(etag)[0], 503)
        self.doc.write_text("# Recovered\n")
        self.assertIn('Recovered', json.loads(self.content(etag)[2])['html'])
        self.doc.write_text("")
        self.assertEqual(json.loads(self.content()[2])['html'], '')

    def test_images_and_restricted_routes(self):
        image = self.root / "image space.svg"
        image.write_text('<svg xmlns="http://www.w3.org/2000/svg"/>')
        status, headers, body = self.request('/image%20space.svg')
        self.assertEqual(status, 200)
        self.assertEqual(headers['Content-Type'], 'image/svg+xml')
        self.assertEqual(body, image.read_bytes())
        self.assertEqual(self.request('/' + quote(self.doc.name))[0], 404)
        self.assertEqual(self.request('/%00.svg')[0], 404)
        self.assertEqual(self.request('/%GG.svg')[0], 404)
        with tempfile.TemporaryDirectory() as outside:
            secret = Path(outside) / 'secret.png'
            secret.write_bytes(b'outside')
            (self.root / 'escape.png').symlink_to(secret)
            self.assertEqual(self.request('/escape.png')[0], 403)
            traversal = '/../' + Path(outside).name + '/secret.png'
            self.assertEqual(self.request(traversal)[0], 403)
        self.assertEqual(self.request('/', {'Host': 'evil.example'})[0], 403)
        self.assertEqual(self.request('/', method='POST')[0], 405)

    def test_cli_errors_and_shutdown(self):
        for args in ([], ['missing.md'], ['--port', '-1', str(self.doc)],
                     ['--port', '65536', str(self.doc)], [str(self.doc), str(self.doc)]):
            result = subprocess.run([BINARY, *args], capture_output=True, timeout=3)
            self.assertNotEqual(result.returncode, 0, args)
        self.assertEqual(subprocess.run([BINARY, '--help'], capture_output=True).returncode, 0)
        self.process.send_signal(signal.SIGINT)
        self.assertEqual(self.process.wait(timeout=3), 0)


class DirectoryTest(ServerCase):
    directory_mode = True

    def setUp(self):
        super().setUp()
        (self.root / 'README.md').write_text('# Root document\n')
        self.folder = self.root / 'Guides с пробелами'
        self.folder.mkdir()
        self.name = 'Guides с пробелами/файл #1 & 50%.md'
        (self.root / self.name).write_text('# Nested document\n\n![image](diagram.svg)\n\n[Root](../README.md)\n')
        (self.folder / 'diagram.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg"/>')
        (self.root / 'empty').mkdir()
        (self.root / '.hidden.md').write_text('hidden')
        (self.root / 'secret.txt').write_text('not markdown')
        (self.root / 'UPPER.MD').write_text('# Uppercase')
        (self.root / 'long.markdown').write_text('# Markdown extension')

    def listing(self, folder=''):
        status, _, body = self.request('/__mdread/list?path=' + quote(folder))
        self.assertEqual(status, 200, body)
        return json.loads(body)['entries']

    def file_content(self, name, etag=None):
        return self.request('/__mdread/content?path=' + quote(name), {'If-None-Match': etag} if etag else {})

    def test_directory_listing_navigation_and_images(self):
        status, _, body = self.request()
        self.assertEqual(status, 200)
        self.assertIn(b'id="explorer"', body)
        self.assertIn(b'data-reading="false"', body)
        entries = self.listing()
        names = [entry['name'] for entry in entries]
        self.assertEqual(names[:2], ['empty', 'Guides с пробелами'])
        for visible in ('README.md', 'UPPER.MD', 'long.markdown'):
            self.assertIn(visible, names)
        self.assertNotIn('.hidden.md', names)
        self.assertNotIn('secret.txt', names)
        folder_url = next(entry['url'] for entry in entries if entry['name'] == self.folder.name)
        self.assertEqual(self.request(folder_url)[0], 200)
        nested = self.listing(self.folder.name)
        self.assertEqual(len(nested), 1)
        self.assertEqual(nested[0]['path'], self.name)
        status, _, body = self.request(nested[0]['url'])
        self.assertEqual(status, 200)
        self.assertIn('data-reading="true"', body.decode())
        self.assertIn('файл #1 &amp; 50%.md', body.decode())
        status, _, body = self.file_content(self.name)
        self.assertEqual(status, 200)
        html = json.loads(body)['html']
        self.assertIn('src="diagram.svg"', html)
        self.assertIn('href="../README.md"', html)
        self.assertEqual(self.request('/files/' + quote(self.folder.name) + '/diagram.svg')[0], 200)
        self.assertEqual(self.request('/files/README.md')[0], 200)
        self.assertEqual(self.listing('empty'), [])
        self.assertEqual(self.request('/files/empty/')[0], 200)

    def test_live_list_and_selected_file(self):
        self.assertEqual(self.file_content(self.name)[0], 200)
        _, headers, _ = self.file_content('README.md')
        etag = headers['ETag']
        self.assertEqual(self.file_content('README.md', etag)[0], 304)
        replacement = self.root / 'save.tmp'
        replacement.write_text('# Changed root\n')
        replacement.replace(self.root / 'README.md')
        status, headers, body = self.file_content('README.md', etag)
        self.assertEqual(status, 200)
        self.assertIn('Changed root', json.loads(body)['html'])
        etag = headers['ETag']
        (self.root / 'README.md').unlink()
        self.assertEqual(self.file_content('README.md', etag)[0], 503)
        self.assertNotIn('README.md', [entry['name'] for entry in self.listing()])
        (self.root / 'README.md').write_text('# Recovered\n')
        self.assertEqual(self.file_content('README.md', etag)[0], 200)
        self.assertIn('README.md', [entry['name'] for entry in self.listing()])
        (self.root / 'New.md').write_text('# New\n')
        self.assertIn('New.md', [entry['name'] for entry in self.listing()])

    def test_path_isolation_and_invalid_requests(self):
        for url in ('/files/../README.md', '/files/%2e%2e/README.md',
                    '/__mdread/content?path=../secret.md', '/__mdread/list?path=..',
                    '/__mdread/content?path=%2Fetc/passwd.md'):
            self.assertEqual(self.request(url)[0], 403, url)
        for url in ('/__mdread/content', '/__mdread/content?path=secret.txt', '/files/secret.txt',
                    '/__mdread/list?path=README.md'):
            self.assertEqual(self.request(url)[0], 404, url)
        with tempfile.TemporaryDirectory() as outside:
            secret = Path(outside) / 'secret.md'
            secret.write_text('# Outside\n')
            (self.root / 'outside.md').symlink_to(secret)
            (self.root / 'outside-dir').symlink_to(outside, target_is_directory=True)
            self.assertNotIn('outside.md', [entry['name'] for entry in self.listing()])
            self.assertNotIn('outside-dir', [entry['name'] for entry in self.listing()])
            self.assertEqual(self.request('/files/outside.md')[0], 200)  # shell remains usable
            self.assertEqual(self.file_content('outside.md')[0], 403)
            self.assertEqual(self.request('/__mdread/list?path=outside-dir')[0], 403)

    def test_resource_versions_and_error_shell(self):
        _, first, _ = self.file_content('README.md')
        for _ in range(10):
            self.file_content(self.name)
            self.assertEqual(self.file_content('README.md', first['ETag'])[0], 304)
        _, headers, _ = self.request('/__mdread/list')
        self.assertEqual(self.request('/__mdread/list', {'If-None-Match':headers['ETag']})[0], 304)
        for name in ('missing.md','big.md'):
            if name == 'big.md':
                with (self.root / name).open('wb') as stream: stream.truncate(17*1024*1024)
            status, _, page = self.request('/files/' + name)
            self.assertEqual(status, 200)
            self.assertIn(b'id="refresh"', page)
            self.assertEqual(self.file_content(name)[0], 503)
        (self.root / 'big.md').write_text('# Recovered')
        self.assertEqual(self.file_content('big.md')[0], 200)

    @unittest.skipIf(os.geteuid() == 0, 'root bypasses directory read permissions')
    def test_content_remains_readable_without_directory_listing_permission(self):
        self.folder.chmod(0o111)
        try:
            self.assertEqual(self.request('/__mdread/list?path=' + quote(self.folder.name))[0], 503)
            self.assertEqual(self.file_content(self.name)[0], 200)
        finally: self.folder.chmod(0o755)

    def test_symlink_races_and_special_files(self):
        import threading
        with tempfile.TemporaryDirectory() as outside:
            secret = Path(outside) / 'secret.md'; secret.write_text('EXTERNAL_SECRET')
            victim = self.root / 'race.md'
            stop = threading.Event()
            def swap():
                while not stop.is_set():
                    tmp = self.root / 'swap'; tmp.write_text('# safe'); tmp.replace(victim)
                    link = self.root / 'swaplink'; link.symlink_to(secret); link.replace(victim)
            thread = threading.Thread(target=swap); thread.start()
            try:
                for _ in range(200):
                    self.assertNotIn(b'EXTERNAL_SECRET', self.file_content('race.md')[2])
            finally:
                stop.set(); thread.join()
            parent = self.root / 'parent'; parent.mkdir(); (parent / 'secret.md').write_text('# local')
            parked = self.root / 'parked'
            stop.clear()
            def swap_parent():
                while not stop.is_set():
                    parent.rename(parked)
                    parent.symlink_to(outside, target_is_directory=True)
                    parent.unlink(); parked.rename(parent)
            thread = threading.Thread(target=swap_parent); thread.start()
            try:
                for _ in range(100): self.assertNotIn(b'EXTERNAL_SECRET', self.file_content('parent/secret.md')[2])
            finally:
                stop.set(); thread.join()
            os.mkfifo(self.root / 'pipe.md')
            self.assertEqual(self.file_content('pipe.md')[0], 503)


class ResilienceTest(ServerCase):
    def test_offline_policy(self):
        process = subprocess.Popen([BINARY, '--offline', '--no-window', str(self.doc)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            ready, _, _ = select.select([process.stdout], [], [], 5)
            self.assertTrue(ready)
            url = urlsplit(process.stdout.readline().strip())
            connection = http.client.HTTPConnection(url.hostname, url.port, timeout=3)
            try:
                connection.request('GET', '/'); response = connection.getresponse()
                policy = response.getheader('Content-Security-Policy'); response.read()
                self.assertIn("img-src 'self' data:;", policy)
                self.assertNotIn('https:', policy)
            finally: connection.close()
        finally:
            process.terminate(); process.communicate(timeout=3)
    def test_single_file_replacement_and_output_budget(self):
        self.content()
        with tempfile.TemporaryDirectory() as outside:
            secret = Path(outside) / 'secret.md'; secret.write_text('EXTERNAL_SECRET')
            self.doc.unlink(); self.doc.symlink_to(secret)
            self.assertEqual(self.content()[0], 403)
            self.doc.unlink(); self.doc.write_text('# Restored')
            self.assertIn('Restored', json.loads(self.content()[2])['html'])
        self.doc.write_text('<' * (2 * 1024 * 1024))
        self.assertEqual(self.content()[0], 503)
        self.assertEqual(self.request()[0], 200)

    def test_slow_clients_do_not_block_other_requests_or_shutdown(self):
        import socket, time, threading
        client = socket.create_connection((self.address.hostname, self.address.port))
        self.addCleanup(client.close)
        stopped = threading.Event()
        def dribble():
            try:
                while not stopped.wait(.08): client.sendall(b'x')
            except OSError: pass
        thread = threading.Thread(target=dribble); thread.start()
        try:
            started = time.monotonic()
            self.assertEqual(self.request()[0], 200)
            self.assertLess(time.monotonic() - started, 1.5)
            self.process.send_signal(signal.SIGTERM)
            self.assertEqual(self.process.wait(timeout=1.5), 0)
        finally:
            stopped.set(); thread.join()

    def test_absolute_header_deadline(self):
        import socket, time, threading
        with socket.create_connection((self.address.hostname, self.address.port)) as client:
            stop = threading.Event()
            def dribble():
                try:
                    while not stop.wait(.1): client.sendall(b'x')
                except OSError: pass
            thread = threading.Thread(target=dribble); thread.start()
            try:
                started = time.monotonic(); client.settimeout(4.5)
                self.assertEqual(client.recv(1), b'')
                self.assertLess(time.monotonic()-started, 4.5)
            finally:
                stop.set(); thread.join()

    def test_nonreading_client_does_not_block_shutdown(self):
        import socket, time
        self.doc.write_text('x' * (3*1024*1024))
        with socket.create_connection((self.address.hostname, self.address.port)) as client:
            client.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
            client.sendall(('GET /__mdread/content HTTP/1.1\r\nHost: '+self.address.netloc+'\r\n\r\n').encode())
            time.sleep(.15)
            self.assertEqual(self.request()[0], 200)
            self.process.terminate(); self.assertEqual(self.process.wait(timeout=1.5), 0)


if __name__ == '__main__':
    unittest.main()
