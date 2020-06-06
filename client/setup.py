from setuptools import setup

requirements = [
    'pyserial',
]

with open('README') as f:
    long_description = f.read()

setup(
    name='removinator',
    version='0.1.0.dev3',
    description='A library for controlling the Smart Card Removinator',
    long_description=long_description,
    url='https://github.com/nkinder/smart-card-removinator',
    author='Smart Card Removinator contributors',
    author_email='nkinder@redhat.com',
    license='APLv2',
    packages=['removinator'],
    classifiers=[
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Testing',
        'Topic :: Software Development :: Libraries :: Python Modules',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
    ],
    install_requires=requirements,
)
