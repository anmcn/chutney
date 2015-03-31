#!/usr/bin/env python
#
# Usage: python setup.py install
#

from distutils.core import setup, Extension
from distutils import sysconfig
import sys

# --------------------------------------------------------------------
# identification

NAME = 'chutney'
VERSION = '1.0.0'
DESCRIPTION = 'A simple and safe pickle parser and generator'
AUTHOR = 'Andrew McNamara', 'andrewm@object-craft.com.au'
HOMEPAGE = 'https://github.com/anmcn/chutney'
DOWNLOAD = 'https://github.com/anmcn/chutney/releases'

sources = [
    'chutney/chutneyparse.c',
    'chutney/chutneygen.c',
    'chutney/chutneyutil.c',
    ]

includes = [
    'chutney',
    ]

defines = [
    ]

# determine suitable defines (based on Python's setup.py file)
#config_h = sysconfig.get_config_h_filename()
#config_h_vars = sysconfig.parse_config_h(open(config_h))
#for feature_macro in ['HAVE_MEMMOVE', 'HAVE_BCOPY']:
#    if config_h_vars.has_key(feature_macro):
#        defines.append((feature_macro, '1'))
#defines.append(('XML_NS', '1'))
#defines.append(('XML_DTD', '1'))
#if sys.byteorder == 'little':
#    defines.append(('BYTEORDER', '1234'))
#else:
#    defines.append(('BYTEORDER', '4321'))
#defines.append(('XML_CONTEXT_BYTES', '1024'))


# --------------------------------------------------------------------
# distutils declarations

chutney_module = Extension(
    'chutney', ['chutney.c'] + sources,
    define_macros=defines,
    include_dirs=includes,
    )

setup(
    author=AUTHOR[0],
    author_email=AUTHOR[1],
    description=DESCRIPTION,
    download_url=DOWNLOAD,
    ext_modules = [chutney_module],
    license='BSD',
    long_description=DESCRIPTION,
    name=NAME,
    platforms='Python 2.3 and later.',
    url=HOMEPAGE,
    version=VERSION,
)
