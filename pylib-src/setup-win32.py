from distutils.core import setup, Extension

skynetExt = [
    Extension(
        'pySkynet',
        sources=['py-skynet.c'],
        libraries=['skynet'],
        extra_compile_args=[],
        extra_link_args=[],
        include_dirs = ['../skynet-src'],
        library_dirs = ['../bin']
    )
]

setup(name="pySkynet", version="1.0", ext_modules=skynetExt)