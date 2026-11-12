"""Test installer decisions and failures without installing system packages."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / 'mac_setup.zsh'
SHIM = r'''
import json
import os
from pathlib import Path
import sys

name = Path(sys.argv[0]).name
args = sys.argv[1:]
with open(os.environ['SETUP_TEST_LOG'], 'a') as log:
    log.write(json.dumps([name, *args]) + '\n')

if name == 'uname':
    print(os.environ.get('SETUP_TEST_OS', 'Darwin'))
elif name == 'xcrun':
    if os.environ.get('SETUP_TEST_NO_CLT'): sys.exit(1)
    print('/mock/SDK' if args == ['--show-sdk-path'] else '/mock/clang++')
elif name == 'xcode-select':
    pass
elif name == 'brew':
    action = args[0]
    if os.environ.get('SETUP_TEST_FAIL') == 'brew-' + action: sys.exit(17)
    if action == 'shellenv': print('export HOMEBREW_PREFIX=/mock/homebrew')
    elif action == '--prefix': print('/mock/homebrew with spaces')
    elif action == 'list':
        if args[-1] in os.environ.get('SETUP_TEST_MISSING', '').split(','): sys.exit(1)
        print(args[-1] + ' 1.0')
elif name == 'sysctl':
    print(os.environ.get('SETUP_TEST_JOBS', '4'))
elif name == 'cmake':
    action = 'build' if '--build' in args else 'install' if '--install' in args else 'configure'
    if os.environ.get('SETUP_TEST_FAIL') == action: sys.exit(19)
else:
    sys.exit('Unexpected command: ' + name)
'''


@unittest.skipIf(os.geteuid() == 0, 'Installer rejects root; run as a normal user')
class SetupTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.project = self.root / 'project with spaces'
        self.project.mkdir()
        self.script = self.project / 'mac_setup.zsh'
        shutil.copy2(SOURCE, self.script)
        (self.project / 'CMakeLists.txt').write_text('# mock project\n')
        self.bin = self.root / 'mock-bin'
        self.bin.mkdir()
        for command in ('uname', 'xcrun', 'xcode-select', 'brew', 'cmake', 'sysctl'):
            shim = self.bin / command
            shim.write_text('#!' + sys.executable + '\n' + SHIM)
            shim.chmod(0o755)
        self.log = self.root / 'commands.jsonl'
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ['PATH'],
                        SETUP_TEST_LOG=str(self.log))

    def run_setup(self, *args, **env):
        return subprocess.run(['/bin/zsh', str(self.script), *args], env=dict(self.env, **env),
                              cwd=self.root, capture_output=True, text=True, timeout=15)

    def calls(self, command=None):
        calls = [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []
        return [call for call in calls if call[0] == command] if command else calls

    def test_default_build_and_install_from_other_directory(self):
        result = self.run_setup()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(any(call[1] == 'install' for call in self.calls('brew')))
        configure, build, install = self.calls('cmake')
        self.assertEqual(configure[configure.index('-S') + 1], str(self.project))
        self.assertEqual(configure[configure.index('-B') + 1], str(self.project / 'build/macos'))
        self.assertIn('*MD4C*', configure)
        self.assertIn('-DCMAKE_PREFIX_PATH=/mock/homebrew with spaces', configure)
        self.assertEqual(build[-2:], ['--parallel', '4'])
        self.assertEqual(install[-2:], ['--prefix', str(Path.home() / '.local')])

    def test_install_only_missing_dependencies_and_custom_paths(self):
        result = self.run_setup('--prefix', 'install prefix', '--build-dir', 'custom build',
                                SETUP_TEST_MISSING='pkgconf,md4c')
        self.assertEqual(result.returncode, 0, result.stderr)
        installs = [call for call in self.calls('brew') if call[1] == 'install']
        self.assertEqual(installs, [['brew', 'install', 'pkgconf', 'md4c']])
        self.assertIn(str(self.root / 'custom build'), self.calls('cmake')[0])
        self.assertEqual(self.calls('cmake')[-1][-1], str(self.root / 'install prefix'))

    def test_build_only_and_invalid_cpu_count(self):
        result = self.run_setup('--build-only', SETUP_TEST_JOBS='invalid')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(self.calls('cmake')), 2)
        self.assertEqual(self.calls('cmake')[-1][-2:], ['--parallel', '2'])

    def test_missing_command_line_tools_stops_before_homebrew(self):
        result = self.run_setup(SETUP_TEST_NO_CLT='1')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.calls('xcode-select'), [['xcode-select', '--install']])
        self.assertEqual(self.calls('brew'), [])
        self.assertEqual(self.calls('cmake'), [])
        self.assertIn('запустите скрипт ещё раз', result.stdout)

    def test_failures_stop_later_steps(self):
        for failure in ('brew-shellenv', 'brew-install', 'brew---prefix', 'configure', 'build', 'install'):
            with self.subTest(failure=failure):
                if self.log.exists(): self.log.unlink()
                result = self.run_setup(SETUP_TEST_FAIL=failure, SETUP_TEST_MISSING='md4c')
                self.assertNotEqual(result.returncode, 0)
                calls = self.calls('cmake')
                if failure.startswith('brew-'): self.assertEqual(calls, [])
                elif failure == 'configure': self.assertEqual(len(calls), 1)
                elif failure == 'build': self.assertEqual(len(calls), 2)

    def test_help_and_bad_arguments_have_no_side_effects(self):
        self.assertEqual(self.run_setup('--help').returncode, 0)
        for args in (('--unknown',), ('--prefix',), ('--build-dir', '--build-only'),
                     ('--build-dir', str(self.project))):
            self.assertNotEqual(self.run_setup(*args).returncode, 0)
        self.assertEqual(self.calls('brew'), [])
        self.assertEqual(self.calls('cmake'), [])

    def test_wrong_platform_stops_before_dependencies(self):
        result = self.run_setup(SETUP_TEST_OS='Linux')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('macOS', result.stderr)
        self.assertEqual(self.calls('brew'), [])


if __name__ == '__main__':
    unittest.main()
