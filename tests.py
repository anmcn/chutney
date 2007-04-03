import sys
import unittest
import chutney


class BasicTests(unittest.TestCase):

    def test_module_const(self):
        self.failUnless(issubclass(chutney.ChutneyError, Exception))
        self.failUnless(issubclass(chutney.UnpickleableError, Exception))
        self.failUnless(issubclass(chutney.UnpicklingError, Exception))
        self.failUnless(callable(chutney.dumps))
        self.failUnless(callable(chutney.loads))


class BasicSuite(unittest.TestSuite):
    tests = [
        'test_module_const',
    ]
    def __init__(self):
        unittest.TestSuite.__init__(self, map(BasicTests, self.tests))


class DumpTests(unittest.TestCase):
    def test_unpickleable(self):
        self.assertRaises(chutney.UnpickleableError, chutney.dumps, min)

    def test_none(self):
        self.assertEqual(chutney.dumps(None), 'N.')

    def test_bool(self):
        self.assertEqual(chutney.dumps(True), '\x88.')
        self.assertEqual(chutney.dumps(False), '\x89.')

    def test_int(self):
        # Protocol 1
        self.assertEqual(chutney.dumps(0), 'M\x00\x00.')
        self.assertEqual(chutney.dumps(1), 'M\x01\x00.')
        self.assertEqual(chutney.dumps(-1), 'J\xff\xff\xff\xff.')
        self.assertEqual(chutney.dumps(sys.maxint), 'J\xff\xff\xff\x7f.')
        self.assertEqual(chutney.dumps(-sys.maxint), 'J\x01\x00\x00\x80.')
# Protocol 0
#        self.assertEqual(chutney.dumps(0), 'I0\n.')
#        self.assertEqual(chutney.dumps(1), 'I1\n.')
#        self.assertEqual(chutney.dumps(-1), 'I-1\n.')
#        self.assertEqual(chutney.dumps(sys.maxint), 'I2147483647\n.')
#        self.assertEqual(chutney.dumps(-sys.maxint), 'I-2147483647\n.')

    def test_float(self):
        self.assertEqual(chutney.dumps(0.0), 
                         'G\x00\x00\x00\x00\x00\x00\x00\x00.')
        self.assertEqual(chutney.dumps(1.0), 
                         'G\x3f\xf0\x00\x00\x00\x00\x00\x00.')
        self.assertEqual(chutney.dumps(1e300),
                         'G\x7e\x37\xe4\x3c\x88\x00\x75\x9c.')
        self.assertEqual(chutney.dumps(1e-300),
                         'G\x01\xa5\x6e\x1f\xc2\xf8\xf3\x59.')
        self.assertEqual(chutney.dumps(-1e300),
                         'G\xfe\x37\xe4\x3c\x88\x00\x75\x9c.')
        self.assertEqual(chutney.dumps(-1e-300),
                         'G\x81\xa5\x6e\x1f\xc2\xf8\xf3\x59.')
# Protocol 0
#        self.assertEqual(chutney.dumps(0.0), 'F0\n.')
#        self.assertEqual(chutney.dumps(1e300), 'F1.0000000000000001e+300\n.')
#        self.assertEqual(chutney.dumps(1e-300), 'F1e-300\n.')
#        self.assertEqual(chutney.dumps(-1e300), 'F-1.0000000000000001e+300\n.')
#        self.assertEqual(chutney.dumps(-1e-300), 'F-1e-300\n.')

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
        self.assertEqual(chutney.dumps((None,1,1.0)), 
                                '(NM\x01\x00G?\xf0\x00\x00\x00\x00\x00\x00t.')
        self.assertEqual(chutney.dumps(((),())), '((t(tt.')

    def test_list(self):
        # Lists are sent as tuples
        self.assertEqual(chutney.dumps([]), '(t.')
        self.assertEqual(chutney.dumps([None,1,1.0]), 
                                '(NM\x01\x00G?\xf0\x00\x00\x00\x00\x00\x00t.')
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
        unittest.TestSuite.__init__(self, map(DumpTests, self.tests))


class LoadTests(unittest.TestCase):
    def error_test(self):
        self.assertRaises(TypeError, chutney.loads, None)
        self.assertRaises(EOFError, chutney.loads, '')
        self.assertRaises(chutney.UnpicklingError, chutney.loads, '.')
        self.assertRaises(chutney.UnpicklingError, chutney.loads, '\xff.')

    def test_none(self):
        self.assertEqual(chutney.loads('N.'), None)

    def test_bool(self):
        self.assertEqual(chutney.loads('\x88.'), True)
        self.assertEqual(chutney.loads('\x89.'), False)

    def test_int(self):
        self.assertEqual(chutney.loads('I0\n.'), 0)
        self.assertEqual(chutney.loads('I1\n.'), 1)
        self.assertEqual(chutney.loads('I-1\n.'), -1)
        self.assertEqual(chutney.loads('I2147483647\n.'), sys.maxint)
        self.assertEqual(chutney.loads('I-2147483647\n.'), -sys.maxint)
        self.assertRaises(chutney.UnpicklingError, chutney.loads, 'Ix\n.')

    def test_binint(self):
        self.assertEqual(chutney.loads('M\x00\x00.'), 0)
        self.assertEqual(chutney.loads('M\x01\x00.'), 1)
        self.assertEqual(chutney.loads('J\xff\xff\xff\xff.'), -1)
        self.assertEqual(chutney.loads('J\xff\xff\xff\x7f.'), sys.maxint)
        self.assertEqual(chutney.loads('J\x01\x00\x00\x80.'), -sys.maxint)
    
    def test_binfloat(self):
        self.assertEqual(chutney.loads('G\x00\x00\x00\x00\x00\x00\x00\x00.'), 0.0)
        self.assertEqual(chutney.loads('G\x3f\xf0\x00\x00\x00\x00\x00\x00.'), 1.0)
        self.assertEqual(chutney.loads('G\x7e\x37\xe4\x3c\x88\x00\x75\x9c.'), 1e300)
        self.assertEqual(chutney.loads('G\x01\xa5\x6e\x1f\xc2\xf8\xf3\x59.'), 1e-300)
        self.assertEqual(chutney.loads('G\xfe\x37\xe4\x3c\x88\x00\x75\x9c.'), -1e300)
        self.assertEqual(chutney.loads('G\x81\xa5\x6e\x1f\xc2\xf8\xf3\x59.'), -1e-300)

    def test_binstring(self):
        self.assertEqual(chutney.loads('U\x00.'), '')
        self.assertEqual(chutney.loads('U\xff' + 'X' * 255 + '.'), 'X' * 255)
        self.assertEqual(chutney.loads('T\x00\x01\x00\x00' + 'X' * 256 + '.'), 
                         'X' * 256)

    def test_unicode(self):
        self.assertEqual(chutney.loads('X\x00\x00\x00\x00.'), u'')
        self.assertEqual(chutney.loads('X\x03\x00\x00\x00abc.'), u'abc')

    def test_tuple(self):
        self.assertRaises(chutney.UnpicklingError, chutney.loads, 't.')
        self.assertEqual(chutney.loads('(t.'), ())
        self.assertEqual(chutney.loads(
                '(NM\x01\x00G?\xf0\x00\x00\x00\x00\x00\x00t.'), (None,1,1.0)) 
        self.assertEqual(chutney.loads('((t(tt.'), ((),()))

    def test_dict(self):
        self.assertEqual(chutney.loads('}.'), {})
        self.assertEqual(chutney.loads('}(u.'), {})
        self.assertEqual(chutney.loads('}(NNu.'), {None: None})
        # SETITEMS with no matching MARK
        self.assertRaises(chutney.UnpicklingError, chutney.loads, 'u.')
        # empty SETITEMS with no dict on stack
        self.assertRaises(chutney.UnpicklingError, chutney.loads, '(u.')
        # non-empty SETITEMS with no dict on stack
        self.assertRaises(chutney.UnpicklingError, chutney.loads, '(NNu.')
        # SETITEMS on a tuple, rather than a dict
        self.assertRaises(TypeError, chutney.loads, '(t(NNu.')
 

class LoadSuite(unittest.TestSuite):
    tests = [
        'error_test',
        'test_none',
        'test_bool',
        'test_int',
        'test_binint',
        'test_binfloat',
        'test_binstring',
        'test_unicode',
        'test_tuple',
        'test_dict',
    ]
    def __init__(self):
        unittest.TestSuite.__init__(self, map(LoadTests, self.tests))


class ChutneySuite(unittest.TestSuite):
    def __init__(self):
        unittest.TestSuite.__init__(self)
        self.addTest(BasicSuite())
        self.addTest(DumpSuite())
        self.addTest(LoadSuite())


suite = ChutneySuite

if __name__ == '__main__':
    if hasattr(sys, 'gettotalrefcount'):
        import gc
        runner = unittest.TextTestRunner()
        suite = ChutneySuite()
        counts = [None] * 5
        for i in xrange(len(counts)):
            runner.run(suite)
            gc.collect()
            counts[i] = sys.gettotalrefcount()
        print counts
    else:
        unittest.main()
