################################################################################
#
# Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
#
################################################################################
"""
This module provide configure file management service in i18n environment.

Authors: wanglong03(wanglong03@baidu.com)
Date:    2015/12/29 14:26:03
"""

import sys
import os
import os.path

import commands

def read_file(file_name, mode='rb'):
    """ read data from file

    Returns:
        data: data read from file
    """

    data = None
    with open(file_name, 'rb') as f:
        data = f.read()

    return data


def get_all_file_names(dir_name):
    """ get all file names under a directory

    Returns:
        file_names: file names list
    """

    file_names = commands.getoutput("find " + dir_name + " -name *.jpg").split('\n')

    return file_names

if __name__ == '__main__':
    files = get_all_file_names('./data')
    print files
    sys.exit()

    file_name = 'test.txt'
    data = read_file(file_name)
    text = data.decode('utf-8')
    length = len(text)
    print "read %d bytes" % (length)

    print text
