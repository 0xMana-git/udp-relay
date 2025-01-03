import socket
import sys
import random
import threading
import client

import time
import queue

packet_queue = {}

RELAY_HOST = "127.0.0.1"
RELAY_PORT = 16969
SRC_PORT = None



class ProxyClient:

    def init_sock(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.bind((self.host, self.port))
    def __init__(self, port, target, relay_client : client.RelayClient):
        self.port = port
        self.host = "0.0.0.0"
        self.target = target
        self.init_sock()
        self.relay_client = relay_client
        self.init_threads()
    
    def init_threads(self):
        threading.Thread(target=self.send_loop, daemon=True).start()
        threading.Thread(target=self.recv_loop, daemon=True).start()
    def send_loop(self):
        global packet_queue
        #if anything comes from the relay client, send
        while True:
            if not self.port in packet_queue.keys():
                packet_queue[self.port] = queue.Queue()
            self.sock.sendto(packet_queue[self.port].get(), self.target)
    def recv_loop(self):
        while True:
            #if anything is sent to the dummies, forward them to the relay client
            pdata = self.sock.recvfrom(65535)
            print(pdata)
            self.relay_client.relay_to(self.port, pdata[0])


    def __del__(self):
        self.sock.close()


def rebuild_dummies(target : tuple, dummies : dict, peers : list, this_port : int, relayc):
    #we want to bind everything BUT our target, since the target does the actual binding
    for p in peers:
        if p == this_port:
            continue
        if not p in dummies.keys():
            dummies[p] = ProxyClient(p, target, relayc)




def queue_up_relay(packet : bytes):
    global packet_queue
    uid = client.decode_u32(packet[:4])
    if not uid in packet_queue.keys():
        packet_queue[uid] = queue.Queue()
    packet_queue[uid].put(packet[4:])


def main():
    start_port = 8720
    port = start_port
    while True:
        relay_client = client.RelayClient(RELAY_HOST, RELAY_PORT, port)
        if relay_client.valid:
            print(f"regisered {port}")
            break
        print("registration fail. retrying...")
        port += 1
    #once we get a valid client, we start to initialize the dummy proxies
    #hopefully this doesnt fuck things up
    peers = relay_client.get_peers()
    relay_client.peers = peers
    relay_client.dispatch_relay = queue_up_relay
    dummies = {}
    #maybe change this to 127.0.0.1
    rebuild_dummies(("127.0.0.1", port), dummies, relay_client.peers, port, relay_client)
    relay_client.start_recv_thread()

    print(f"Proxy server is ready. Client can now bind to port {port}")
    while True:
        #sure i could make a ondispatch hook but im lazy
        relay_client.send_packet(client.GET_PEERLIST, b"")
        time.sleep(4)
        rebuild_dummies(("127.0.0.1", port), dummies, relay_client.peers, port, relay_client)


if __name__ == "__main__":
    main()
