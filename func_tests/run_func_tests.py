#!/usr/bin/env python3
import argparse
import os
import shutil
import logging

from func_tests import run_tests, rc_out_err_to_str
from utils import execute_shell, cat, find_files
from ansistrm import setup_logging

try:
    from config import TOOL_EXEC, IIMC_EXEC
except ImportError:
    logging.getLogger().fatal('Either config.py does not exist, '
                              'or it does not define strings TOOL_EXEC, IIMC_EXEC.\n'
                              'To fix this, create config.py (in {me}), and define:\n'.
                                  format(me=os.path.dirname(os.path.abspath(__file__))) +
                              'TOOL_EXEC="<path to demiurge executable>", \n'
                              'IIMC_EXEC="<path to IIMC executable>".\n'
                              'Exiting.')
    exit(-1)


def is_realizable(test_file:str):
    # The absolute_file_path structure is:
    # <something>/real/file_name.hoa
    # <something>/unreal/file_name.hoa
    status = test_file.split('/')[-2]
    assert status in ['real', 'unreal'], 'spec status is unknown: ' + status
    return status == 'real'


def run_sdf(test_file, signals_file, result_file, tool_args:str):
    cmd_run = f'{TOOL_EXEC} {test_file} -p {signals_file} -o {result_file} {tool_args}'
    logger.debug('executing: ' + cmd_run)
    return execute_shell(cmd_run)


# noinspection PyUnusedLocal
def check_answer(test_file:str, result_file:str, rc, out, err):
    exit_status_realizable = 10
    exit_status_unrealizable = 20
    assert rc in [exit_status_realizable, exit_status_unrealizable], rc_out_err_to_str(rc, out, err)

    expected = [exit_status_unrealizable, exit_status_realizable][is_realizable(test_file)]

    if rc != expected:
        status_to_str = {exit_status_realizable: 'realizable',
                         exit_status_unrealizable: 'unrealizable'}
        out = f'wrong realizability status: should be {status_to_str[expected]},' \
              f'but the tool found it {status_to_str[rc]}'
        return 1, out, None

    return 0, None, None


def model_check(file_to_check) -> bool:
    # Below 'pi' specifies the property to model check
    cmd = f'{IIMC_EXEC} {file_to_check} --pi 0'
    logger.debug('executing ' + cmd)
    rc, out, err = execute_shell(cmd)
    assert rc == 0, rc_out_err_to_str(rc, out, err)

    last_line = tuple(l.strip() for l in out.splitlines() if l.strip())[-1]
    assert last_line in ['0', '1'], rc_out_err_to_str(rc, out, err)

    return "0" == last_line  # the last line "0" means the circuit is correct


def check_answer_with_mc(test_file, result_file, rc, out, err):
    rc, out, err = check_answer(test_file, result_file, rc, out, err)
    if rc != 0:
        return rc, out, err

    if not is_realizable(test_file):
        return 0, None, None

    tmp_file_name = result_file + '.aag'   # because IIMC understands only AIGER extensions
    shutil.copy(result_file, tmp_file_name)

    is_correct = model_check(tmp_file_name)
    os.remove(tmp_file_name)

    if is_correct:
        return 0, None, None
    else:
        return 1, 'The circuit is buggy', None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Functional tests runner (Note: I run sdf-hoa with \'-k 4\')')

    parser.add_argument('--mc',
                        action='store_true',
                        required=False,
                        default=False,
                        help='model check the result (default: not set)')
    parser.add_argument('-a',
                        '--args',
                        required=False,
                        default='',
                        help='arguments to pass to the tool. '
                             'Enclose the arguments in "".')

    parser.add_argument('-v', '--verbose', action='count', default=0)

    args = parser.parse_args()

    tests_dir = os.path.dirname(os.path.abspath(__file__))
    test_files = find_files(f'{tests_dir}/hoa-files/real/', extension='hoa') +\
                 find_files(f'{tests_dir}/hoa-files/unreal/', extension='hoa')

    logger = setup_logging(args.verbose)

    failed_tests = run_tests(test_files,
                             lambda test_file, result_file: run_sdf(test_file,
                                                                    test_file[:test_file.rfind(".hoa")]+".part",
                                                                    result_file,
                                                                    args.args + ' -k 4'),
                             [check_answer, check_answer_with_mc][args.mc],
                             True,
                             logger)

    exit(len(failed_tests))
