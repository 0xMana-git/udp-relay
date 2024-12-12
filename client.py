import socket
import random
import threading


GET_PEERLIST = 0
RELAY_PACKET = 1
REGISTER_ID = 2
UNREGISTER_ID = 3
INVALID = 255

import sys

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def dbgprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)
def get_msg(packet : bytes):
    return packet[0]
def get_headerless(packet : bytes):
    return packet[5:]

def parse_peers(packet : bytes):
    assert(len(packet) % 4 == 0)
    res = []
    for i in range(0, len(packet), 4):
        res.append(decode_u32(packet[i:i+4]))
    return res

def print_peers(peers : list):
    dbgprint(f"Peers {str(peers)}")
def encode_u32(v : int) -> bytes:
    return v.to_bytes(4, "little")
#maybe its i32? idk
def decode_u32(b : bytes) -> int:
    return int.from_bytes(b, "little", signed=False)

def print_relay(packet : bytes):
    
    print(f"Recieved {packet[4:].decode()} @ {decode_u32(packet[:4])}")


class RelayClient:
    def init_sock(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.bind((self.client_host, self.client_port))
    def send_packet(self, message : int, data : bytes):
        res = message.to_bytes(1)
        tot_size = len(data) + 1 + 4
        res += encode_u32(tot_size)
        res += data
        assert(len(res) == tot_size)
        sent_size = self.sock.sendto(res, (self.host, self.port))
        return sent_size == tot_size
    
    def recv_packet_raw(self):
        return self.sock.recvfrom(65535)[0]
    
    def dispatch_peerlist(self, packet : bytes):
        tmp_peers = parse_peers(packet)
        if self.peers == tmp_peers:
            return
        self.peers = tmp_peers
        self.on_peerlist_update(self.peers)

    

    def dispatch_packet(self, packet : bytes):
        message = get_msg(packet)
        headerless = get_headerless(packet)
        match message:
            case 0:
                self.dispatch_peerlist(headerless)
            case 1:
                self.dispatch_relay(headerless)
            case 2:
                pass
            case 3:
                pass
            case _:
                print("Invalid packet from server!")
    
    def recv_loop(self):
        while True:
            self.dispatch_packet(self.recv_packet_raw())

    def register(self) -> bool:
        self.send_packet(REGISTER_ID, encode_u32(self.id))
        packet = self.recv_packet_raw()
        if get_msg(packet) != REGISTER_ID:
            return False
        packet_headerless = get_headerless(packet)
        success_byte = packet_headerless[0]
        if success_byte == 0x55:
            return True
        return False


    def relay_to(self, dst_id : int, payload : bytes):
        assert(self.valid)
        #make packet
        pl = encode_u32(dst_id)
        pl += encode_u32(self.id)
        pl += payload
        self.send_packet(RELAY_PACKET, pl)
    
    #DONT CALL THIS IF CLIENT IS ALREADY REGISTERED
    def get_peers(self):
        #no need for valid client
        self.send_packet(GET_PEERLIST, b"")
        packet = self.recv_packet_raw()
        assert(get_msg(packet) == GET_PEERLIST)
        self.peers = parse_peers(get_headerless(packet))
        self.on_peerlist_update(self.peers)
        return self.peers
    


    def __init__(self, host : str, port : int, id : int, client_host : str = None, client_port : int = None) -> None:
        self.host = host
        self.port = port
        self.id = id
        self.client_host = "0.0.0.0" if client_host == None else client_host
        self.client_port = random.randint(40000, 65535) if client_port == None else client_port
        self.init_sock()
        self.valid = False
        dbgprint(f"Registering id {id}" )
        if self.register():
            self.valid = True
        dbgprint(f"client host: {self.client_host}; port: {self.client_port}")
        self.dispatch_relay = print_relay
        self.on_peerlist_update = print_peers
    def start_recv_thread(self):
        
        self.recv_thread = threading.Thread(target=self.recv_loop, daemon=True)
        self.recv_thread.start()




        