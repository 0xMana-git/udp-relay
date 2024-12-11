import socket
import sys
import random
import threading

RELAY_HOST = "127.0.0.1"
RELAY_PORT = 16969
SRC_PORT = None


src_id = int(sys.argv[1])


def reverse_endianness(n : int) -> int:
    return int.from_bytes(n.to_bytes(2), "little")





def recv_loop(s : socket.socket):
    while(True):
        data = s.recvfrom(65536)
        print("Recieved: " + data[0][4:].decode())

def send_packet(sock : socket.socket, payload : bytes):
    sock.sendto(payload, (RELAY_HOST, RELAY_PORT))

def make_packet(dst_id : int, payload : bytes) -> bytes:
    res : bytes = b""
    #mode = relay
    res += b"\x01"
    res += dst_id.to_bytes(4, "little")
    res += src_id.to_bytes(4, "little")
    res += payload
    
    assert(len(res) < 65536)
    return res



def main():
    global SRC_PORT
    SRC_PORT = random.randint(40000, 65535)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind(("0.0.0.0", reverse_endianness(SRC_PORT)))
    #advertise
    #this packet will get dropped, but this peer will be recognized
    send_packet(sock, make_packet(0, b""))
    #now send/recv fr
    threading.Thread(target=recv_loop, args=(sock,),daemon=True).start()
    while True:
        send_packet(sock, make_packet(input("target id: "), input("payload: ").encode()))



if __name__ == "__main__":
    main()
