import sys
import unittest
import chutney


class BasicTests(unittest.TestCase):

    def test_module_const(self):
        self.failUnless(issubclass(chutney.ChutneyError, Exception))
        self.failUnless(issubclass(chutney.UnpickleableError, Exception))
        self.failUnless(callable(chutney.dumps))
        self.failUnless(callable(chutney.loads))


class BasicSuite(unittest.TestSuite):
    tests = [
        'test_module_const',
    ]
    def __init__(self):
        unittest.TestSuite(self, map(BasicTests, self.tests))


class DumpTests(unittest.TestCase):
    def test_none(self):
        self.assertRaises(chutney.UnpickleableError, chutney.dumps(min))

    def test_none(self):
        self.assertEqual(chutney.dumps(None), 'N.')

    def test_bool(self):
        self.assertEqual(chutney.dumps(True), '\x88.')
        self.assertEqual(chutney.dumps(False), '\x89.')

    def test_int(self):
#        self.assertEqual(chutney.dumps(0), 'M\x01\x00.')
#        self.assertEqual(chutney.dumps(1), 'M\x01\x00.')
#        self.assertEqual(chutney.dumps(-1), 'J\0xf\0xf\0xf\0xff')
#        self.assertEqual(chutney.dumps(sys.maxint), 'J\xff\xff\xff\x7f.')
#        self.assertEqual(chutney.dumps(-sys.maxint), 'J\x01\x00\x00\x80.')
        self.assertEqual(chutney.dumps(0), 'I0\n.')
        self.assertEqual(chutney.dumps(1), 'I1\n.')
        self.assertEqual(chutney.dumps(-1), 'I-1\n.')
        self.assertEqual(chutney.dumps(sys.maxint), 'I2147483647\n.')
        self.assertEqual(chutney.dumps(-sys.maxint), 'I-2147483647\n.')

    def test_float(self):
        self.assertEqual(chutney.dumps(0.0), 'F0\n.')
        self.assertEqual(chutney.dumps(1e300), 'F1.0000000000000001e+300\n.')
        self.assertEqual(chutney.dumps(1e-300), 'F1e-300\n.')
        self.assertEqual(chutney.dumps(-1e300), 'F-1.0000000000000001e+300\n.')
        self.assertEqual(chutney.dumps(-1e-300), 'F-1e-300\n.')

    def test_string(self):
        self.assertEqual(chutney.dumps(''), 'U\x00.')
        self.assertEqual(chutney.dumps('X' * 255), 'U\xff' + 'X' * 255 + '.')
        self.assertEqual(chutney.dumps('X' * 256), 
                                       'T\x00\x01\x00\x00' + 'X' * 256 + '.')

    def test_unicode(self):
        self.assertEqual(chutney.dumps(u''), 'X\x00\x00\x00\x00.')
        self.assertEqual(chutney.dumps(u'abc'), 'X\x03\x00\x00\x00abc.')

    def test_tuple(self):
        self.assertEqual(chutney.dumps(()), '(t.')
        self.assertEqual(chutney.dumps((None,1,1.0)), '(NI1\nF1\nt.')
        self.assertEqual(chutney.dumps(((),())), '((t(tt.')

    def test_list(self):
        # Lists are sent as tuples
        self.assertEqual(chutney.dumps([]), '(t.')
        self.assertEqual(chutney.dumps([None,1,1.0]), '(NI1\nF1\nt.')
        self.assertEqual(chutney.dumps([(),[]]), '((t(tt.')

    def test_dict(self):
        self.assertEqual(chutney.dumps({}), '}.')
        self.assertEqual(chutney.dumps({None: None}), '}(NNu.')


class DumpSuite(unittest.TestSuite):
    tests = [
        'test_unpickleable',
        'test_none',
        'test_bool',
        'test_int',
        'test_float',
        'test_string',
        'test_unicode',
        'test_tuple',
        'test_list',
        'test_dict',
    ]
    def __init__(self):
        unittest.TestSuite(self, map(DumpTests, self.tests))


class ChutneySuite(unittest.TestSuite):
    def __init__(self):
        unittest.TestSuite(self)
        self.addTest(BasicSuite())
        self.addTest(DumpSuite())


suite = ChutneySuite

if __name__ == '__main__':
    unittest.main()
