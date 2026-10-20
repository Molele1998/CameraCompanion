"""Check a public source tree or source ZIP without printing matched values."""
import argparse
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import zipfile
from PIL import Image

TEXT = {'.ino', '.h', '.cpp', '.c', '.py', '.md', '.txt', '.json', '.yml', '.yaml'}
IGNORED = {'.git', '.build', '.cache', '__pycache__'}
FORBIDDEN = {'.bin', '.elf', '.map', '.dump', '.log', '.bak', '.pem', '.key', '.pfx',
             '.p12', '.db', '.sqlite', '.csv', '.exe', '.dll', '.zip', '.ttf', '.otf'}
PRIVATE_NAMES = {'auth.json', 'credentials.json', 'cookies.json', 'testconfig.h', 'arduino_secrets.h'}
RULES = {
    'absolute_windows_path': re.compile(r'(?i)\b[A-Z]:[\\/](?!/)'),
    'home_directory': re.compile(r'/(?:home|Users)/[^/\s]+'),
    'device_mac': re.compile(r'(?i)\b(?:[0-9a-f]{2}:){5,7}[0-9a-f]{2}\b'),
    'email_address': re.compile(r'(?i)\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b'),
    'jwt_literal': re.compile(r'eyJ[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]{8,}'),
    'api_key_literal': re.compile(r'\bsk-[A-Za-z0-9_-]{20,}\b'),
    'private_key': re.compile(r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'),
    'credential_in_url': re.compile(r'https?://[^/\s]+:[^/@\s]+@'),
    'secret_url_query': re.compile(r'''https?://[^\s'"<>]+[?&](?:token|key|secret|password|auth)=[^\s'"<>]+''', re.I),
}

def private_json(value):
    if isinstance(value, dict):
        if any(str(key).lower() in {'ssid', 'password', 'access_token', 'refresh_token', 'api_key', 'client_secret'}
               and isinstance(item, str) and bool(item) for key, item in value.items()):
            return True
        return any(private_json(item) for item in value.values())
    return isinstance(value, list) and any(private_json(item) for item in value)

def inspect(name, data):
    path = PurePosixPath(name)
    findings = []
    if path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name:
        return ['unsafe_path']
    lower = path.name.lower()
    if path.suffix.lower() in FORBIDDEN or lower in PRIVATE_NAMES or '.private.' in lower or lower.startswith('.env'):
        findings.append('private_or_generated_file')
    if path.suffix.lower() in TEXT or lower in {'license', 'notice', '.gitignore'}:
        try:
            content = data.decode('utf-8-sig')
        except UnicodeDecodeError:
            return findings + ['non_utf8_text']
        for label, rule in RULES.items():
            if label == 'email_address' and 'licenses' in path.parts:
                continue  # Keep upstream public copyright attribution intact.
            if rule.search(content):
                findings.append(label)
        for label in ['USERNAME', 'COMPUTERNAME']:
            local_value = os.environ.get(label, '')
            if len(local_value) >= 6 and local_value.casefold() in content.casefold():
                findings.append('local_identity_marker')
        if path.suffix.lower() == '.json':
            try:
                if private_json(json.loads(content)):
                    findings.append('private_configuration_data')
            except (ValueError, TypeError):
                findings.append('invalid_json')
    elif path.suffix.lower() == '.png':
        try:
            with Image.open(io.BytesIO(data)) as picture:
                if any(key not in {'dpi', 'gamma', 'srgb', 'transparency'} for key in picture.info):
                    findings.append('image_metadata')
                picture.verify()
        except (OSError, ValueError):
            findings.append('invalid_image')
    else:
        findings.append('unreviewed_file_type')
    return findings

def self_test():
    cases = [
        ('sample.txt', ('Q:' + chr(92) + 'Users' + chr(92) + 'example').encode(), 'absolute_windows_path'),
        ('sample.txt', ':'.join(['aa', 'bb', 'cc', 'dd', 'ee', 'ff']).encode(), 'device_mac'),
        ('renamed.json', json.dumps({'refresh_' + 'token': 'synthetic-value'}).encode(), 'private_configuration_data'),
        ('../outside.md', b'test', 'unsafe_path'),
        ('copy.bin', b'not-a-real-image', 'private_or_generated_file'),
        ('sample.txt', ('sk-' + 'a' * 24).encode(), 'api_key_literal'),
    ]
    for name, data, expected in cases:
        assert expected in inspect(name, data), expected
    image = Image.new('RGB', (2, 2));buffer = io.BytesIO()
    from PIL.PngImagePlugin import PngInfo
    info = PngInfo();info.add_text('Comment', 'synthetic private metadata')
    image.save(buffer, format='PNG', pnginfo=info)
    assert 'image_metadata' in inspect('image.png', buffer.getvalue())
    assert not inspect('README.md', b'Camera Companion public source')
    print('PASS: privacy negative fixtures and clean control.')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
    findings = [];checked = 0
    if args.archive:
        with zipfile.ZipFile(args.archive) as archive:
            assert archive.testzip() is None, 'ZIP CRC failure'
            entries = archive.infolist()
            assert len(entries) == len({entry.filename for entry in entries}), 'Duplicate ZIP entries'
            assert sum(entry.file_size for entry in entries) <= 32 * 1024 * 1024, 'Archive exceeds review limit'
            for entry in entries:
                if entry.is_dir():
                    continue
                if stat.S_ISLNK(entry.external_attr >> 16) or any(part in IGNORED for part in PurePosixPath(entry.filename).parts):
                    findings.append({'file': entry.filename, 'check': 'local_or_linked_file'})
                for label in inspect(entry.filename, archive.read(entry)):
                    findings.append({'file': entry.filename, 'check': label})
                checked += 1
    else:
        for path in sorted(args.root.rglob('*')):
            relative = path.relative_to(args.root)
            if any(part in IGNORED for part in relative.parts):
                continue
            if path.is_symlink():
                findings.append({'file': relative.as_posix(), 'check': 'symlink'})
            elif path.is_file():
                for label in inspect(relative.as_posix(), path.read_bytes()):
                    findings.append({'file': relative.as_posix(), 'check': label})
                checked += 1
    print(json.dumps({'clean': not findings, 'checked_files': checked, 'findings': findings}, ensure_ascii=False, indent=2))
    raise SystemExit(1 if findings else 0)

if __name__ == '__main__':
    main()
