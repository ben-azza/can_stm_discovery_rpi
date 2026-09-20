#!/usr/bin/env python3
"""
Noeud ROS2 : ecoute le bus CAN et attend l'accuse de reception envoye
par le STM32 Nucleo L476RG une fois la position atteinte.

Trame attendue :
    ID = 0x200
    Data[0] = 0xAA  -> "position atteinte"

Publie ensuite un message String sur le topic /position_status.
"""

import can
import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class CanAckListener(Node):
    def __init__(self):
        super().__init__('can_ack_listener')

        self.declare_parameter('can_channel', 'can0')
        self.declare_parameter('can_id', 0x200)
        self.declare_parameter('ack_byte', 0xAA)

        channel = self.get_parameter('can_channel').get_parameter_value().string_value
        self.expected_id = self.get_parameter('can_id').get_parameter_value().integer_value
        self.ack_byte = self.get_parameter('ack_byte').get_parameter_value().integer_value

        self.publisher_ = self.create_publisher(String, 'position_status', 10)

        try:
            self.bus = can.interface.Bus(channel=channel, interface='socketcan')
        except Exception as e:
            self.get_logger().error(f"Impossible d'ouvrir le bus CAN '{channel}': {e}")
            raise

        self.get_logger().info(
            f"En attente de l'accuse de reception du STM32 (ID=0x{self.expected_id:X})..."
        )

        # Polling non bloquant du bus CAN a 10 Hz
        self.timer = self.create_timer(0.1, self.check_can)

    def check_can(self):
        msg = self.bus.recv(timeout=0.0)
        if msg is None:
            return

        if msg.arbitration_id == self.expected_id and len(msg.data) >= 1:
            if msg.data[0] == self.ack_byte:
                out = String()
                out.data = "La position est atteinte"
                self.publisher_.publish(out)
                self.get_logger().info("La position est atteinte")


def main(args=None):
    rclpy.init(args=args)
    node = CanAckListener()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
