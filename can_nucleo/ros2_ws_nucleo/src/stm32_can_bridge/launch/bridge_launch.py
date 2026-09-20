from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('x', default_value='0.0',
                               description='Position X a envoyer au STM32'),
        DeclareLaunchArgument('y', default_value='0.0',
                               description='Position Y a envoyer au STM32'),
        DeclareLaunchArgument('theta', default_value='0.0',
                               description='Orientation theta a envoyer au STM32'),
        DeclareLaunchArgument('can_channel', default_value='can0',
                               description='Interface CAN socketcan a utiliser'),

        # Demarre l'ecoute AVANT l'envoi pour ne rater aucun ACK
        Node(
            package='stm32_can_bridge',
            executable='can_ack_listener',
            name='can_ack_listener',
            output='screen',
            parameters=[{
                'can_channel': LaunchConfiguration('can_channel'),
                 'ack_byte': 1,
            }],
        ),
        Node(
            package='stm32_can_bridge',
            executable='can_position_publisher',
            name='can_position_publisher',
            output='screen',
            parameters=[{
                'x': LaunchConfiguration('x'),
                'y': LaunchConfiguration('y'),
                'theta': LaunchConfiguration('theta'),
                'can_channel': LaunchConfiguration('can_channel'),
            }],
        ),
    ])
