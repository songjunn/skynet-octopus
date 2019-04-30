from distutils.core import setup, Extension

skynetExt = [
    Extension(
        'pySkynet',
        sources=['py-skynet.c'],
        libraries=[],
        extra_compile_args=['-std=gnu99', '-Wall', '-g', '-fPIC'],
        include_dirs = ['../skynet-src'],
        library_dirs = []
    )
]

setup(name="pySkynet", version="1.0", ext_modules=skynetExt)