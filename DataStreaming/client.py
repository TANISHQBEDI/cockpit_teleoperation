import os
import socket
import threading
import time


def heartbeat_loop(client_socket, interval):
    while True:
        try:
            client_socket.sendall(b"PONG")
            time.sleep(interval)
        except OSError:
            break


def main():
    server_host = os.getenv("SERVER_HOST", "127.0.0.1")
    server_port = int(os.getenv("SERVER_PORT", "12345"))
    heartbeat_interval = float(os.getenv("HEARTBEAT_INTERVAL", "1.0"))
    watchdog_timeout = float(os.getenv("WATCHDOG_TIMEOUT", "3.0"))

    while True:
        try:
            client_socket = socket.create_connection((server_host, server_port), timeout=2)
            client_socket.settimeout(1.0)
            print(f"Connected to server at {server_host}:{server_port}")

            heartbeat_thread = threading.Thread(
                target=heartbeat_loop,
                args=(client_socket, heartbeat_interval),
                daemon=True,
            )
            heartbeat_thread.start()

            last_server_activity = time.monotonic()
            while True:
                try:
                    data = client_socket.recv(1024)
                    if not data:
                        break

                    last_server_activity = time.monotonic()
                    message = data.decode('utf-8', errors='ignore').strip()
                    print(f"Received data: {message}", flush=True)

                    if time.monotonic() - last_server_activity > watchdog_timeout:
                        break

                except socket.timeout:
                    if time.monotonic() - last_server_activity > watchdog_timeout:
                        print("Watchdog timeout: no server activity.")
                        break
                    continue
                except OSError:
                    print("Socket error from server.")
                    break

        except (ConnectionRefusedError, TimeoutError, OSError):
            print("Connection failed, retrying...")
            time.sleep(1)
        finally:
            try:
                client_socket.close()
                print("Disconnected from server.")
            except Exception:
                pass


if __name__ == "__main__":
    main()
