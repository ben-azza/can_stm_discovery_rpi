#!/usr/bin/env python3
"""
Noeud ROS2 interactif : demande a l'utilisateur de taper une chaine de
caracteres dans le terminal, puis l'envoie au STM32 Nucleo L476RG via CAN.

La chaine est decoupee en trames de 8 octets maximum (limite CAN standard),
avec un octet nul '\0' ajoute a la fin pour marquer la fin du message.

Trame envoyee :
    ID = 0x150
    Data = jusqu'a 8 octets de la chaine (ASCII), la derniere trame
           contenant le caractere nul de fin.

Usage :
    ros2 run stm32_can_bridge can_string_sender
    (puis taper du texte au clavier, Entree pour envoyer, 'quit' pour arreter)
"""

import can
import rclpy
from rclpy.node import Node

CAN_ID_STRING = 0x150
CHUNK_SIZE = 8


class CanStringSender(Node):
    def __init__(self):
        super().__init__('can_string_sender')

        self.declare_parameter('can_channel', 'can0')
        self.declare_parameter('can_id', CAN_ID_STRING)

        channel = self.get_parameter('can_channel').get_parameter_value().string_value
        self.can_id = self.get_parameter('can_id').get_parameter_value().integer_value

        try:
            self.bus = can.interface.Bus(channel=channel, interface='socketcan')
        except Exception as e:
            self.get_logger().error(f"Impossible d'ouvrir le bus CAN '{channel}': {e}")
            raise

        self.get_logger().info(
            "Pret. Tape un message et appuie sur Entree pour l'envoyer au STM32."
        )
        self.get_logger().info("Tape 'quit' pour arreter.")

    def send_string(self, text: str):
        # On ajoute un octet nul de fin, puis on decoupe en morceaux de 8 octets
        payload = text.encode('ascii', errors='replace') + b'\x00'

        for i in range(0, len(payload), CHUNK_SIZE):
            chunk = payload[i:i + CHUNK_SIZE]
            msg = can.Message(
                arbitration_id=self.can_id,
                data=chunk,
                is_extended_id=False,
            )
            try:
                self.bus.send(msg)
            except can.CanError as e:
                self.get_logger().error(f"Echec d'envoi CAN : {e}")
                return

        self.get_logger().info(f'Message envoye : "{text}"')

    def run_interactive_loop(self):
        while rclpy.ok():
            try:
                text = input("Message a envoyer > ")
            except (EOFError, KeyboardInterrupt):
                break

            if text.strip().lower() == 'quit':
                break

            if text == '':
                continue

            self.send_string(text)


def main(args=None):
    rclpy.init(args=args)
    node = CanStringSender()
    try:
        node.run_interactive_loop()
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
