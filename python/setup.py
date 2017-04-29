from setuptools import setup, find_packages

setup(
    name="python",
    version="1.0",
    packages=['cv_kernel'] + find_packages('cv2'),
    entry_points={
        'console_scripts': [
            'cv_client_test = cv_kernel.cv_client:test',
            'cv_supervisor_test = cv_kernel.cv_supervisor:test'
        ]
    }
)
