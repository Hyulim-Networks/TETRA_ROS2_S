#!/usr/bin/env python3

import os

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.substitutions import LaunchConfiguration
from launch.actions import DeclareLaunchArgument

def generate_launch_description():
    return LaunchDescription([

        Node(
            package='tcp_bridge',
            executable='tcp_server_node',
            name='tcp_server_node',
            output='screen'
        )
    ]
)
