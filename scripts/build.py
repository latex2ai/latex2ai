# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# MIT License
#
# Copyright (c) 2020 Ivo Steinbrecher
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# -----------------------------------------------------------------------------

"""
Automatically build LaTeX2AI and create a arcive with the executables.
"""

# Import python modules.
import os
import shutil
import subprocess
from zipfile import ZipFile
from pathlib import Path
from create_headers import get_git_sha


def get_git_tag_or_hash():
    """
    Return the commit, if the commit has a tag, return the tag.
    """

    # Get current sha.
    git_sha = get_git_sha()

    # Get all tags and their sha.
    process = subprocess.Popen(['git', 'show-ref', '--tags'],
        stdout=subprocess.PIPE)
    out, _err = process.communicate()
    tags = out.decode('UTF-8').strip().split('\n')
    tags = [line.split(' ') for line in tags]
    for sha, tag in tags:
        if sha == git_sha:
            # This commit has a tag, return the tag.
            return tag.split('/')[-1]
    return git_sha[:7]


def get_illustrator_version(path):
    """
    Return a short name of the Illustrator version.
    """

    split_path = os.path.normpath(path).split(os.sep)
    version_dir = {
        'Adobe Illustrator CS6 SDK': 'IllustratorCS6',
        'Adobe Illustrator CC 2018 SDK': 'IllustratorCC2018',
        'Adobe Illustrator 2021 SDK': 'Illustrator2021'
        }
    illustrator_version = None
    for path_part in split_path:
        if path_part in version_dir.keys():
            if illustrator_version is None:
                illustrator_version = version_dir[path_part]
            else:
                raise ValueError(
                    'Could not find the AI version in {}'.format(path))
    if illustrator_version is None:
        raise ValueError('Could not find the AI version in {}'.format(path))
    else:
        return illustrator_version


def build_solution(repository_dir, build_type, git_info, illustrator_version):
    """
    Build the solution and compress the executables into a zip file.
    """

    # Build the solution, for this we have to set the build type environment
    # variable.
    script_dir = os.path.join(repository_dir, 'scripts')
    os.chdir(script_dir)
    os.environ['LaTeX2AI_build_type'] = build_type
    return_value = subprocess.call(['compile_solution.bat'])

    if return_value == 0:
        # The build passed, compress the executables.
        executable_dir = os.path.realpath(os.path.join(
            repository_dir, '..\output', 'win', 'x64', build_type))
        os.chdir(executable_dir)
        executables = [
            'LaTeX2AI.aip',
            'LaTeX2AIForms.exe'
            ]
        zip_name = ('LaTeX2AI_' + git_info + '_' + build_type.lower()
            + '_' + illustrator_version + '.zip')
        with ZipFile(zip_name, mode='w') as zf:
            for f in executables:
                zf.write(f)
        return os.path.join(executable_dir, zip_name)
    else:
        raise ValueError('Could not build solution in {}'.format(
            repository_dir))


def clean_repository(repository_dir):
    """
    Check if the repositroy is clean.
    """

    # Check if the repository is clean.
    os.chdir(repository_dir)
    out = subprocess.check_output(['git', 'status', '--porcelain'])
    if out == b'':
        return True
    else:
        return False


def correct_git_state(repository_dir, git_info_other):
    """
    Check if the repositroy is at the correct state.
    """

    # Check if the repository is clean.
    if not clean_repository(repository_dir):
        raise ValueError('Repository {} not clean'.format(repository_dir))

    # Check if we are at the right commit.
    git_info = get_git_tag_or_hash()
    if (git_info_other is not None) and (not git_info == git_info_other):
        # We are in a clean state and the version does not match, so pull
        # the new version.
        os.chdir(repository_dir)
        subprocess.call(['git', 'fetch', 'origin'])
        subprocess.call(['git', 'fetch', 'origin', '--tags'])
        subprocess.call(['git', 'checkout', git_info_other])
        git_info = get_git_tag_or_hash()
        if not git_info == git_info_other:
            raise ValueError(('Could not obtain the correct commit {} in {}'
                ).format(git_info_other, repository_dir))
    return git_info


def build_all_types(repository_dir, git_info_other=None):
    """
    Build all build types in a repository.
    """

    git_info = correct_git_state(repository_dir, git_info_other)
    illustrator_version = get_illustrator_version(repository_dir)

    # Remove output directory.
    output_dir = os.path.realpath(
        os.path.join(repository_dir, '..\output'))
    try:
        if os.path.isdir(output_dir):
            shutil.rmtree(output_dir)
    except OSError as _e:
        return []

    build_types = [
        'Release',
        'Debug'
        ]
    zip_files = []
    for build_type in build_types:
        zip_files.append(build_solution(repository_dir, build_type, git_info,
            illustrator_version))
    return zip_files


if __name__ == '__main__':
    """Execution part of script"""

    # Get base directory.
    file_path = os.path.realpath(os.path.join(os.getcwd(), __file__))
    script_dir = os.path.dirname(file_path)
    base_dir = os.path.dirname(script_dir)

    zip_files = []

    if 'LATEX2AI_REPOSITORIES' in os.environ.keys():
        # Build all.
        repositories = os.environ['LATEX2AI_REPOSITORIES'].strip().split(';')
        if repositories[-1] == '':
            del repositories[-1]
        os.chdir(base_dir)
        git_info = get_git_tag_or_hash()
        for repository in repositories:
            zip_files.extend(build_all_types(repository,
                git_info_other=git_info))
    else:
        zip_files.extend(build_all_types(base_dir))

    # Move all zip files to this dir.
    zip_dir = script_dir = os.path.join(script_dir, 'executables')
    Path(zip_dir).mkdir(exist_ok=True)
    for zip_file in zip_files:
        os.replace(zip_file,
            os.path.join(zip_dir, os.path.basename(zip_file)))

    input()
