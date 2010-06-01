#!/usr/bin/env python

import sys
sys.path.append('.')

from kernel.resource import Resource
from kernel import kernel_data
from kernel.helpers.bin_sm import BinarySemaphore

class TaskAtHD(Resource):
    ''' '''

    def __init__(self, opts = {}):
        opts['name'] = 'task_at_hd_' + str(kernel_data.RID)
        Resource.__init__(self, opts)
        self.sem = BinarySemaphore()
        self.write_info = opts['write_info']

    def free(self):
        return self.sem.s == 1
