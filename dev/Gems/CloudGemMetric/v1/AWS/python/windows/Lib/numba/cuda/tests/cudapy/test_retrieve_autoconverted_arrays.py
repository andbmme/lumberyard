from __future__ import print_function, absolute_import, division

import numpy as np

from numba import cuda
from numba import unittest_support as unittest
from numba.cuda.args import wrap_arg
from numba.cuda.testing import SerialMixin


class DefaultIn(object):
    def prepare_args(self, ty, val, **kwargs):
        return ty, wrap_arg(val, default=cuda.In)


def nocopy(kernel):
    kernel.extensions.append(DefaultIn())
    return kernel


def set_array_to_three(arr):
    arr[0] = 3


def set_record_to_three(rec):
    rec[0]['b'] = 3


recordtype = np.dtype(
    [('b', np.int32)],
    align=True
)


class TestRetrieveAutoconvertedArrays(SerialMixin, unittest.TestCase):
    def setUp(self):
        self.set_array_to_three = cuda.jit(set_array_to_three)
        self.set_array_to_three_nocopy = nocopy(cuda.jit(set_array_to_three))
        self.set_record_to_three = cuda.jit(set_record_to_three)
        self.set_record_to_three_nocopy = nocopy(cuda.jit(set_record_to_three))

    def test_array_inout(self):
        host_arr = np.zeros(1, dtype=np.int64)
        self.set_array_to_three(cuda.InOut(host_arr))
        self.assertEqual(3, host_arr[0])

    def test_array_in(self):
        host_arr = np.zeros(1, dtype=np.int64)
        self.set_array_to_three(cuda.In(host_arr))
        self.assertEqual(0, host_arr[0])

    def test_array_in_from_config(self):
        host_arr = np.zeros(1, dtype=np.int64)
        self.set_array_to_three_nocopy(host_arr)
        self.assertEqual(0, host_arr[0])

    def test_array_default(self):
        host_arr = np.zeros(1, dtype=np.int64)
        self.set_array_to_three(host_arr)
        self.assertEqual(3, host_arr[0])

    def test_record_in(self):
        host_rec = np.zeros(1, dtype=recordtype)
        self.set_record_to_three(cuda.In(host_rec))
        self.assertEqual(0, host_rec[0]['b'])

    def test_record_inout(self):
        host_rec = np.zeros(1, dtype=recordtype)
        self.set_record_to_three(cuda.InOut(host_rec))
        self.assertEqual(3, host_rec[0]['b'])

    def test_record_default(self):
        host_rec = np.zeros(1, dtype=recordtype)
        self.set_record_to_three(host_rec)
        self.assertEqual(3, host_rec[0]['b'])

    def test_record_in_from_config(self):
        host_rec = np.zeros(1, dtype=recordtype)
        self.set_record_to_three_nocopy(host_rec)
        self.assertEqual(0, host_rec[0]['b'])


if __name__ == '__main__':
    unittest.main()
