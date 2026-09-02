import os
import socket
import threading
import time
import pygame


class Joy:
    def __init__(self):
        pygame.init()
        pygame.joystick.init()

        self.mapped_value1 = 1000
        self.mapped_value2 = 1000
        self.transmit_value = ""

        if pygame.joystick.get_count() == 0:
            return

        self.gamepad = pygame.joystick.Joystick(0)
        self.gamepad.init()

        self.server_host = os.getenv("SERVER_HOST", "0.0.0.0")
        self.server_port = int(os.getenv("SERVER_PORT", "12345"))
        self.watchdog_timeout = float(os.getenv("WATCHDOG_TIMEOUT", "3.0"))

        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.server_host, self.server_port))
        self.server_socket.listen(5)

        self.flag_r_pressed = False
        self.flag_l_pressed = False
        print(f"Server listening on {self.server_host}:{self.server_port}")

    def handle_client(self, client_socket, client_address):
        # print(f"Client connected: {client_address}")
        client_socket.settimeout(0.001)
        last_seen = time.monotonic()
        last_send_time = time.monotonic()

        while True:
            try:
                if time.monotonic() - last_seen > self.watchdog_timeout:
                    print(f"Watchdog timeout for {client_address}; disconnecting.")
                    break

                try:
                    payload = client_socket.recv(16)
                    if payload:
                        message = payload.decode("utf-8", errors="ignore").strip()
                        if message:
                            last_seen = time.monotonic()
                except (BlockingIOError, InterruptedError):
                    pass
                except ConnectionResetError:
                    break

                pygame.event.pump()
                self.transmit_value = "0"
                button_map = {
                    19: "f",
                    20: "b",
                    0: "p",
                    1: "m",
                    10: "x",
                    2: "o",
                    4: "r",
                    5: "l",
                    11: "i",
                    3: "u",
                    23: "s",
                    24: "e",
                    7: "a",
                    6: "d",
                }
                for btn, code in button_map.items():
                    if self.gamepad.get_button(btn):
                        self.transmit_value = code
                        break

                try:
                    hat_state = self.gamepad.get_hat(0)
                except Exception:
                    hat_state = (0, 0)

                if hat_state == (0, 1):
                    self.transmit_value = "up"
                elif hat_state == (0, -1):
                    self.transmit_value = "dn"

                self.mapped_value1 = int(round(self.gamepad.get_axis(1), 3) * 1000)
                self.mapped_value2 = int(round(self.gamepad.get_axis(0), 3) * 1000)
                self.data = "{},{},{}".format(self.mapped_value1, self.mapped_value2, self.transmit_value)
                client_socket.sendall(self.data.encode())
                print(self.data, flush=True)

                elapsed = time.monotonic() - last_send_time
                if elapsed < 0.03:
                    time.sleep(0.03 - elapsed)
                last_send_time = time.monotonic()

            except KeyboardInterrupt:
                break
            except ConnectionError:
                # print("Client disconnected.")
                break
            except OSError:
                # print("Socket error, disconnecting.")
                break

        client_socket.close()
        # print(f"Client disconnected: {client_address}")

    def start(self):
        while True:
            client_socket, client_address = self.server_socket.accept()
            client_handler = threading.Thread(
                target=self.handle_client,
                args=(client_socket, client_address),
            )
            client_handler.start()


def main():
    joy = Joy()
    joy.start()


if __name__ == "__main__":
    main()
