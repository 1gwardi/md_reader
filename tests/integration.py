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
    def test_render_and_page(self):
        status, headers, body = self.request()
        self.assertEqual(status, 200)
        self.assertIn("тест с пробелами.md", body.decode())
        self.assertIn("script-src 'self'", headers["Content-Security-Policy"])
        status, headers, body = self.content()
        html = json.loads(body)["html"]
        for fragment in ('<h1>Привет</h1>', '<strong>Жирный</strong>', '<table>', 'type="checkbox"', 'class="language-cpp"'):
            self.assertIn(fragment, html)
        self.assertNotIn('<script>', html)
        self.assertIn('&lt;script&gt;', html)
        self.assertEqual(self.content(headers['ETag'])[0], 304)
        for path, mime in (("style.css", "text/css"), ("app.js", "text/javascript")):
            status, headers, body = self.request("/__mdread/" + path)
            self.assertEqual(status, 200)
            self.assertTrue(headers["Content-Type"].startswith(mime))
            self.assertTrue(body)

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
        self.assertIn('Nested document', body.decode())
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
                    '/files/missing.md', '/__mdread/list?path=missing', '/__mdread/list?path=README.md'):
            self.assertEqual(self.request(url)[0], 404, url)
        with tempfile.TemporaryDirectory() as outside:
            secret = Path(outside) / 'secret.md'
            secret.write_text('# Outside\n')
            (self.root / 'outside.md').symlink_to(secret)
            (self.root / 'outside-dir').symlink_to(outside, target_is_directory=True)
            self.assertNotIn('outside.md', [entry['name'] for entry in self.listing()])
            self.assertNotIn('outside-dir', [entry['name'] for entry in self.listing()])
            self.assertEqual(self.request('/files/outside.md')[0], 403)
            self.assertEqual(self.file_content('outside.md')[0], 403)
            self.assertEqual(self.request('/__mdread/list?path=outside-dir')[0], 403)


if __name__ == '__main__':
    unittest.main()
