#!/usr/bin/env python

import sys
sys.path.append('.')

from kernel.resource import Resource
from kernel import kernel_data
from kernel.helpers.bin_sm import BinarySemaphore

class TaskAtInput(Resource):
    ''' 
      TaskAtInput resource is created for every task found.
      It should be deleted or not freed by read process.
    '''

    def __init__(self, opts = {}):
        opts['name'] = 'task_at_input_' + str(kernel_data.RID)
        Resource.__init__(self, opts)
        self.sem = BinarySemaphore()
        self.file_path = opts['file_path']

    def free(self):
        return self.sem.s == 1
