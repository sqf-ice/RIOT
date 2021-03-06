#!/usr/bin/env python3

# Copyright (C) 2017 HAW-Hamburg.de
#
# This file is subject to the terms and conditions of the GNU Lesser
# General Public License v2.1. See the file LICENSE in the top level
# directory for more details.

import os
import sys


class TestFailed(Exception):
    pass


def testfunc(child):

    child.expect(u"Tests for FatFs over VFS - test results will be printed in "
                 "the format test_name:result\r\n")

    while True:
        res = child.expect([u"[^\n]*:\[OK\]\r\n",
                            u"Test end.\r\n",
                            u".[^\n]*:\[FAILED\]\r\n",
                            u".*\r\n"])
        if res > 1:
            raise TestFailed(child.after.split(':', 1)[0] + " test failed!")
        elif res == 1:
            break


if __name__ == "__main__":
    sys.path.append(os.path.join(os.environ['RIOTTOOLS'], 'testrunner'))
    import testrunner
    sys.exit(testrunner.run(testfunc))
