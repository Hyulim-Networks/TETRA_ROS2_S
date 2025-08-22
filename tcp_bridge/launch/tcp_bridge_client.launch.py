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
            executable='tcp_client_node',
            name='tcp_client_node',
            output='screen',
            parameters=[
                {"server_ip": "192.168.123.53"},
                {"server_port": 9000}
            ]
        )
    ])
