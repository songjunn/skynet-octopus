from distutils.core import setup, Extension

skynetExt = [
    Extension(
        'pySkynet',
        sources=['py-skynet.c'],
        libraries=[],
        extra_compile_args=[],
        extra_link_args=['-shared', '-fPIC'],
        include_dirs = ['../skynet-src'],
        library_dirs = []
    )
]

setup(name="pySkynet", version="1.0", ext_modules=skynetExt)