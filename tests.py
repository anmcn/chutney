import sys
import unittest
import chutney


class BasicTests(unittest.TestCase):

    def test_module_const(self):
        self.failUnless(issubclass(chutney.ChutneyError, Exception))
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
        self.assertEqual(chutney.dumps(None), 'N.')

    def test_int(self):
        self.assertEqual(chutney.dumps(0), 'I0\n.')
        self.assertEqual(chutney.dumps(1), 'I1\n.')
        self.assertEqual(chutney.dumps(-1), 'I-1\n.')
        self.assertEqual(chutney.dumps(sys.maxint), 'I2147483647\n.')
        self.assertEqual(chutney.dumps(-sys.maxint), 'I-2147483647\n.')


class DumpSuite(unittest.TestSuite):
    tests = [
        'test_none',
        'test_int',
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
