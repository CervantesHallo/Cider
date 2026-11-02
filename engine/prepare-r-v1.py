#!/usr/bin/env python3
"""Reconstruct the recorded R v1 source inputs without building or executing Wine."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tarfile


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        while True:
            chunk = stream.read(1024 * 1024)
            if not chunk:
                return value.hexdigest()
            value.update(chunk)


def stream_digest(stream):
    value = hashlib.sha256()
    while True:
        chunk = stream.read(1024 * 1024)
        if not chunk:
            return value.hexdigest()
        value.update(chunk)


def relative(name):
    path = PurePosixPath(name)
    if not path.parts or path.is_absolute() or '..' in path.parts:
        raise ValueError('Unsafe relative input path: ' + name)
    return path


def prepare(args):
    repo = Path(__file__).resolve().parent.parent
    plan_path = args.plan.resolve(strict=True)
    plan_bytes = plan_path.read_bytes()
    plan = json.loads(plan_bytes)
    if plan['schema'] != 'cider.r-v1-source-plan/v1' or plan['default_eligible'] is not False:
        raise ValueError('Expected an explicitly nondefault R v1 source plan')
    patches = []
    for item in plan['patches']:
        path = repo.joinpath(*relative(item['path']).parts).resolve(strict=True)
        if not path.is_relative_to(repo):
            raise ValueError('Patch input mismatch: ' + item['path'])
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError('Patch input mismatch: ' + item['path'])
        patches.append((item, data))
    archive = args.archive.resolve(strict=True)
    prefix = str(relative(plan['source']['subdir']))
    output = args.output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError('Output must be a new directory; existing work is never replaced')
    if not output.parent.is_dir():
        raise ValueError('Create the output parent first')
    # Hash and extract through the same descriptor, not two pathname opens.
    with archive.open('rb') as incoming_archive:
        archive_before = os.fstat(incoming_archive.fileno())
        if stream_digest(incoming_archive) != plan['source']['sha256']:
            raise ValueError('Source archive SHA256 mismatch')
        incoming_archive.seek(0)
        return extract(args, plan, plan_bytes, patches, prefix, output,
                       incoming_archive, archive_before)


def extract(args, plan, plan_bytes, patches, prefix, output, incoming_archive, archive_before):
    with tarfile.open(fileobj=incoming_archive, mode='r:gz') as source:
        members = source.getmembers()
        names = set()
        for member in members:
            path = relative(member.name)
            if path.parts[0] != prefix or member.name in names:
                raise ValueError('Unexpected or duplicate archive member: ' + member.name)
            names.add(member.name)
            if not (member.isdir() or member.isfile() or member.issym() or member.islnk()):
                raise ValueError('Unsupported source member: ' + member.name)
        source_bytes = sum(member.size for member in members if member.isfile())
        peak = source_bytes * 2 + sum(len(data) for _, data in patches)
        free = shutil.disk_usage(output.parent).free
        if free < peak + 20 * 1024**3:
            raise ValueError('Insufficient free space for expected peak plus 20 GiB')
        output.mkdir(mode=0o700)
        tree = output / 'source'
        tree.mkdir()
        receipt = dict(schema='cider.r-v1-source-preparation/v1', state='running', passed=False,
                       scope='Source only; no compiler, Wine, Windows or runtime qualification',
                       planSHA256=hashlib.sha256(plan_bytes).hexdigest(),
                       preparerSHA256=digest(Path(__file__).resolve()),
                       sourceArchiveSHA256=plan['source']['sha256'], sourceCommit=plan['source']['commit'],
                       sourceBytes=source_bytes, expectedPeakBytes=peak, initialFreeBytes=free,
                       patches=[], expectedSourceChecks={})
        receipt_path = output / 'preparation.json'

        def save():
            receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')

        save()
        try:
            links = []
            for member in members:
                path = relative(member.name)
                destination = tree.joinpath(*path.parts[1:])
                if member.isdir():
                    destination.mkdir(parents=True, exist_ok=True)
                elif member.isfile():
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    with source.extractfile(member) as incoming, destination.open('xb') as outgoing:
                        shutil.copyfileobj(incoming, outgoing)
                    destination.chmod(member.mode & 0o777)
                    os.utime(destination, (member.mtime, member.mtime))
                else:
                    links.append((member, destination))
            # Never extract a regular file through an archive-provided symlink.
            for member, destination in links:
                destination.parent.mkdir(parents=True, exist_ok=True)
                if member.issym():
                    target = destination.parent / member.linkname
                    if not target.resolve().is_relative_to(tree):
                        raise ValueError('Source symlink leaves output: ' + member.name)
                    destination.symlink_to(member.linkname)
                else:
                    link = relative(member.linkname)
                    if link.parts[0] != prefix:
                        raise ValueError('Source hardlink outside archive root')
                    target = tree.joinpath(*link.parts[1:])
                    if not target.is_file() or target.is_symlink():
                        raise ValueError('Hardlink target is not an extracted regular file')
                    os.link(target, destination)
            if any(not destination.resolve().is_relative_to(tree) for _, destination in links):
                raise ValueError('Source link chain leaves output')
            archive_after = os.fstat(incoming_archive.fileno())
            if (archive_before.st_size, archive_before.st_mtime_ns, archive_before.st_ctime_ns) != (
                    archive_after.st_size, archive_after.st_mtime_ns, archive_after.st_ctime_ns):
                raise ValueError('Source archive changed while extracting')
            for item, data in patches:
                result = subprocess.run(['/usr/bin/patch', '-d', str(tree), '-p1', '--batch',
                                         '--forward', '--fuzz=0'], input=data,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60)
                row = dict(path=item['path'], sha256=item['sha256'], exitCode=result.returncode,
                           stdout=result.stdout.decode('utf-8', 'replace'),
                           stderr=result.stderr.decode('utf-8', 'replace'))
                receipt['patches'].append(row)
                save()
                text = (row['stdout'] + row['stderr']).lower()
                if result.returncode or 'offset' in text or 'fuzz' in text:
                    raise ValueError('Patch did not apply exactly: ' + item['path'])
            for name, expected in plan['expected_source_sha256'].items():
                path = tree.joinpath(*relative(name).parts)
                actual = digest(path)
                receipt['expectedSourceChecks'][name] = dict(expected=expected, actual=actual,
                                                           matched=actual == expected)
            if not all(item['matched'] for item in receipt['expectedSourceChecks'].values()):
                raise ValueError('Reconstructed source differs from qualified component source')
            receipt.update(passed=True, state='complete')
        except BaseException as error:
            receipt.update(state='failed', error=repr(error))
            raise
        finally:
            save()
    return receipt_path


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', type=Path,
                        default=Path(__file__).resolve().parent / 'recipes/cider-r-v1-source-20261102.json')
    parser.add_argument('--archive', type=Path, required=True, help='Existing pinned source archive; no download')
    parser.add_argument('--output', type=Path, required=True, help='New owned output directory')
    print(prepare(parser.parse_args()))
