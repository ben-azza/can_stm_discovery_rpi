import os
from glob import glob
from setuptools import setup

package_name = 'stm32_can_bridge'

setup(
    name=package_name,
    version='0.0.1',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.py')),
    ],
    install_requires=['setuptools', 'python-can'],
    zip_safe=True,
    maintainer='you',
    maintainer_email='you@example.com',
    description='Pont ROS2 <-> STM32 Nucleo L476RG via bus CAN (MCP2515 CAN HAT)',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'can_position_publisher = stm32_can_bridge.can_position_publisher:main',
            'can_ack_listener = stm32_can_bridge.can_ack_listener:main',
        ],
    },
)
