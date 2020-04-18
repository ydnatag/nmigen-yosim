from setuptools import setup, find_packages

setup(
    name='nmigen_yosim',
    version='0.0.1',
    description='nMigen simulator backend',
    classifiers=[
        'Development Status :: Alpha',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
    ],

    packages=find_packages(),
    python_requires='>=3.6',
    install_requires=['nmigen'],  # Optional
    package_data={'nmigen_yosim': ['wrapper.cc.j2']},
)
