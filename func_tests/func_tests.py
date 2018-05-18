import shutil

from logging import Logger
from os import makedirs

from utils import get_tmp_dir_name


def rc_out_err_to_str(rc, out, err):
    return 'rc:{rc}\n' \
           'out:{out}\n' \
           'err:{err}'.format(rc=rc, out=out or '<empty>', err=err or '<empty>')


def _generate_name(output_dir, test):
    name = '{output_dir}/{test_last}'.format(output_dir=output_dir,
                                             test_last=test.split('/')[-1])
    return name


def run_tests(test_files,
              run_tool:"func(file, result_file)->(rc, out, err)",
              check_answer:"func(test_file, result_file, rc, out, err)->(rc, out, err)",
              stop_on_error,
              logger:Logger,
              output_folder=None) -> list:
    """
    :param output_folder: if not None, intermediate results are saved there.
                          Files in that folder will be overwritten.
    """

    if output_folder:
        output_dir = output_folder
        makedirs(output_dir, exist_ok=True)
    else:
        output_dir = get_tmp_dir_name()

    logger.info("using " + output_dir + " as the temporal folder")

    failed_tests = list()
    for test in test_files:
        logger.info('testing {test}'.format(test=test))

        log_stream = open(_generate_name(output_dir, test) + '.log', 'w')

        result_file = _generate_name(output_dir, test) + '.model'
        r_rc, r_out, r_err = run_tool(test, result_file)

        logger.debug(rc_out_err_to_str(r_rc, r_out, r_err))
        print(rc_out_err_to_str(r_rc, r_out, r_err),
              file=log_stream)

        c_rc, c_out, c_err = check_answer(test, result_file, r_rc, r_out, r_err)
        logger.debug(rc_out_err_to_str(c_rc, c_out, c_err))
        print(rc_out_err_to_str(c_rc, c_out, c_err),
              file=log_stream)

        if c_rc != 0:
            logger.info('    FAILED')
            failed_tests.append(test)
            if stop_on_error:
                break

    if failed_tests:
        logger.info('The following tests failed: %s \n%s',
                    ''.join('\n    ' + t for t in failed_tests),
                    'See logs in ' + output_dir)
    else:
        logger.info('ALL TESTS PASSED')

    if not output_folder and not failed_tests:
        shutil.rmtree(output_dir)

    return failed_tests
