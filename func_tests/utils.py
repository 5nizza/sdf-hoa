import os
import shlex
import subprocess
from tempfile import mkstemp, mkdtemp

from typing import List


def find_files(directory:str, extension:str='', ignore_mark=None) -> List[str]:
    """
    Walk recursively :arg directory,
      ignoring dirs that contain file named :arg ignore_mark,
    and return all files matching :arg extension
    """

    if not extension.startswith('.'):
        extension = '.' + extension

    matching_files = list()
    for top, subdirs, files in os.walk(directory, followlinks=True):
        if ignore_mark in files:
            subdirs.clear()
            continue
        for f in files:
            if f.endswith(extension):
                matching_files.append(os.path.join(top, f))
    return matching_files


def execute_shell(cmd, input='', cmd_uses_shell_tricks=False):
    """
    Execute cmd, send input to stdin.
    :param cmd_uses_shell_tricks: set True when need redirecting and pipe-lining
    :return: returncode, stdout, stderr.
    """

    proc_stdin = subprocess.PIPE if input != '' and input is not None else None
    proc_input = input if input != '' and input is not None else None

    if not cmd_uses_shell_tricks:   # TODO: detect automatically
        args = shlex.split(cmd)
    else:
        args = cmd

    p = subprocess.Popen(args,
                         stdin=proc_stdin,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         shell=cmd_uses_shell_tricks
                         )

    out, err = p.communicate(proc_input)

    return p.returncode, \
           str(out, encoding='utf-8'), \
           str(err, encoding='utf-8')


def get_tmp_file_name():
    (fd, name) = mkstemp()
    os.close(fd)
    return name


def get_tmp_dir_name():
    return mkdtemp()


def cat(test):
    f = open(test)
    res = f.readlines()
    f.close()
    return res
