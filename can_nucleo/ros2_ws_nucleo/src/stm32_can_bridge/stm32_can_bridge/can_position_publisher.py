#!/usr/bin/env python3
"""
Noeud ROS2 : envoie une position (x, y, theta) au STM32 Nucleo L476RG
via le bus CAN (MCP2515 CAN HAT sur le RPi5).

Trame envoyee :
    ID = 0x100
    Data[0:2] = x     (int16, little-endian, valeur reelle * 100)
    Data[2:4] = y     (int16, little-endian, valeur reelle * 100)
    Data[4:6] = theta (int16, little-endian, valeur reelle * 100)
    Data[6:8] = 0x00, 0x00 (padding, DLC=8)

Usage :
    ros2 run stm32_can_bridge can_position_publisher --ros-args -p x:=1.5 -p y:=2.3 -p theta:=0.78
"""

import struct

import can
import rclpy
from rclpy.node import Node


class CanPositionPublisher(Node):
    def __init__(self):
        super().__init__('can_position_publisher')

        self.declare_parameter('x', 0.0)
        self.declare_parameter('y', 0.0)
        self.declare_parameter('theta', 0.0)
        self.declare_parameter('can_channel', 'can0')
        self.declare_parameter('can_id', 0x100)

        x = self.get_parameter('x').get_parameter_value().double_value
        y = self.get_parameter('y').get_parameter_value().double_value
        theta = self.get_parameter('theta').get_parameter_value().double_value
        channel = self.get_parameter('can_channel').get_parameter_value().string_value
        self.can_id = self.get_parameter('can_id').get_parameter_value().integer_value

        try:
            self.bus = can.interface.Bus(channel=channel, interface='socketcan')
        except Exception as e:
            self.get_logger().error(f"Impossible d'ouvrir le bus CAN '{channel}': {e}")
            raise

        self.send_position(x, y, theta)

    def send_position(self, x: float, y: float, theta: float):
        x_raw = int(round(x * 100))
        y_raw = int(round(y * 100))
        th_raw = int(round(theta * 100))

        # Saturation pour rester dans un int16 signe
        x_raw = max(-32768, min(32767, x_raw))
        y_raw = max(-32768, min(32767, y_raw))
        th_raw = max(-32768, min(32767, th_raw))

        data = struct.pack('<hhh', x_raw, y_raw, th_raw) + b'\x00\x00'

        msg = can.Message(
            arbitration_id=self.can_id,
            data=data,
            is_extended_id=False,
        )

        try:
            self.bus.send(msg)
            self.get_logger().info(
                f'Position envoyee sur ID=0x{self.can_id:X} : '
                f'x={x:.2f} y={y:.2f} theta={theta:.2f}'
            )
        except can.CanError as e:
            self.get_logger().error(f"Echec d'envoi CAN : {e}")


def main(args=None):
    rclpy.init(args=args)
    node = CanPositionPublisher()
    # Un seul envoi puis on quitte (noeud "one-shot")
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
