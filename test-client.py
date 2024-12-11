import socket
import sys
import random
import threading
import client

RELAY_HOST = "127.0.0.1"
RELAY_PORT = 16969
SRC_PORT = None


src_id = int(sys.argv[1])




def main():
    c = client.RelayClient(RELAY_HOST, RELAY_PORT, src_id)
    if not c.valid:
        print("registration fail. exiting...")
        return
    c.start_recv_thread()
    while True:
        c.relay_to(int(input("relay to: ")), input("payload: ").encode())
        c.send_packet(client.GET_PEERLIST, b"")



if __name__ == "__main__":
    main()
