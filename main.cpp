
// Server side implementation of UDP client-server model 
#include <bits/stdc++.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <unordered_set>
#include <mutex>
#include <thread>
#include <cstdint>




std::string addr_to_string(const in_addr& addr) {
    char client_str[128];
    inet_ntop(AF_INET, &(addr), client_str, INET_ADDRSTRLEN);
    return client_str;

}

std::string sockaddr_to_hostport(const sockaddr_in& addr) {
    return addr_to_string(addr.sin_addr) + ":" + std::to_string(ntohs(addr.sin_port));
}


using uint64 = unsigned long long;
using byte = unsigned char;


using id_t = unsigned int32_t;
using packet_size_t = unsigned int32_t;

enum MESSAGE_TYPE : byte {
    GET_PEERLIST = 0,
    RELAY_PACKET = 1,
    REGISTER_ID = 2,
    UNREGISTER_ID = 3,
    INVALID = 255
};





//MAXIMUM size, not necessarily actual packet size
constexpr size_t buf_size = 65535;


struct PacketBase {
    MESSAGE_TYPE type = INVALID;
    packet_size_t size = 0;

} __attribute__((packed));

//
struct ClientPacket : public PacketBase {

    union {
        //GetPeerlist 
        //
        //RelayPacket
        struct {
            id_t dst, src;
            char content[buf_size - sizeof(PacketBase) - sizeof(dst) - sizeof(src)];
        } __attribute__((packed));
        //RegisterID/UnregisterID
        struct {
            id_t reg_id;
            char pad[buf_size - sizeof(PacketBase) - sizeof(reg_id)];
        } __attribute__((packed));
        
    };
    constexpr size_t get_relayed_size() {
        return sizeof(content) + sizeof(src) + sizeof(MESSAGE_TYPE);
    }
} __attribute__((packed));


struct ServerPacket : public PacketBase {
    constexpr static size_t max_peers = 16382;
    union {
        //GetPeerlist 
        id_t peer_data[max_peers];

        //RegisterID/UnregisterID
        byte success;
        
        //RelayPacket
        struct {
            id_t src = 0;
            char content[buf_size - sizeof(PacketBase) - sizeof(src)];
        } __attribute__((packed));
        
        
    } __attribute__((packed));;
    //Peer index constructor
    ServerPacket(const std::vector<id_t>& peer_ids) {
        assert(peer_ids.size() < max_peers);
        for(size_t i = 0; i < peer_ids.size(); i++) {
            peer_data[i] = peer_ids[i];
        }
        type = GET_PEERLIST;
        size = peer_ids.size() * sizeof(id_t) + sizeof(PacketBase);

    }
    ServerPacket(const ClientPacket& relaySource) {
        assert(relaySource.type == RELAY_PACKET);
        type = RELAY_PACKET;
        //Size of this entire packet should be content + src + header
        //Which is equivalent to source size - dst
        size = relaySource.size - sizeof(relaySource.dst);
        size_t content_size = size - sizeof(PacketBase) - sizeof(src);
        //Make sure it actually fits in the buffer
        assert(sizeof(content) >= content_size);
        memcpy(content, relaySource.content, content_size);
        this->src = relaySource.src;
    }

    ServerPacket(bool is_register, bool success) {
        if(success)
            this->success = 0x55;
        else  
            this->success = 0x00;
        type = is_register ? REGISTER_ID : UNREGISTER_ID;
        size = sizeof(this->success) + sizeof(PacketBase);
    }

    ServerPacket() {
        type = INVALID;
        size = 0xffffffff;
    }

}__attribute__((packed));

static_assert(sizeof(ClientPacket) == buf_size);
static_assert(sizeof(ServerPacket) == buf_size);


struct SockData {
    sockaddr_in sock = {0};
    int sockfd = 0;
    int send(const ServerPacket& packet) const {
        assert(packet.size < buf_size);
        return sendto(sockfd, &packet, packet.size, 0, (sockaddr*)&sock, sizeof(sock));
    }
    bool send_check(const ServerPacket& packet) const {
        return send(packet) == packet.size;
    }
    SockData(const sockaddr_in& sock, int fd) {
        this->sock = sock;
        this->sockfd = fd;
    }
    std::string to_string() const {
        return sockaddr_to_hostport(sock);
    }
};



//udp port
constexpr int RELAY_PORT = 16969;

std::mutex peers_lock;
std::mutex peerspacket_lock;

std::unordered_map<uint64, SockData> peers;

decltype(std::cerr)& dbgcerr = std::cerr;


ServerPacket peers_packet;

//CALL only when parent locks peers
ServerPacket get_peers() {
    ServerPacket res;
    std::vector<id_t> peers_v;
    for(const auto& it : peers) 
        peers_v.push_back(it.first);
    return ServerPacket(peers_v);
}

void add_peer(uint64 id, const SockData& peer_sock) {
    std::lock_guard lock(peers_lock);
    std::lock_guard lockp(peerspacket_lock);
    //there really is no point keeping
    peers.erase(id);
    peers.emplace(id, peer_sock);
    
    peers_packet = get_peers();
}

void remove_peer(uint64 id) {
    std::lock_guard lock(peers_lock);
    std::lock_guard lockp(peerspacket_lock);
    //there really is no point keeping
    peers.erase(id);

    
    peers_packet = get_peers();
}


const SockData* find_peer(uint64 id) {
    std::lock_guard lock(peers_lock);
    auto it = peers.find(id);
    if(it == peers.end())
        return nullptr;
    return &(it->second);
}






bool send_peerdata(const SockData& peer_sock) {
    std::lock_guard lock(peerspacket_lock);
    return peer_sock.send_check(peers_packet);
    
}

bool dispatch_message(const SockData& peer_sock, const ClientPacket& packet) {
    bool success;
    dbgcerr << "packet type " << (int)packet.type << std::endl;
    switch(packet.type) {
        case GET_PEERLIST:
            return send_peerdata(peer_sock);
        case RELAY_PACKET:
            if(auto peer = find_peer(packet.reg_id); peer != nullptr) {
                return peer->send_check(ServerPacket(packet));
            }
            return false;
        case REGISTER_ID:
        dbgcerr << "registering " << (int)packet.reg_id << std::endl;
            if(find_peer(packet.reg_id) == nullptr){
                add_peer(packet.reg_id, peer_sock);
                dbgcerr << "peer added. sending response..." << std::endl;
                return peer_sock.send_check(ServerPacket(true, true));
            }
            dbgcerr << "peer already exists. sending fail response..." << std::endl;
            return peer_sock.send_check(ServerPacket(true, false));
                
        case UNREGISTER_ID:
            if(find_peer(packet.reg_id) != nullptr){
                remove_peer(packet.reg_id);
                return peer_sock.send_check(ServerPacket(false, true));
            }
            return peer_sock.send_check(ServerPacket(false, false));

        //drop
        default:
            return true;


    }

}

int main() { 
    int sockfd; 
    
    struct sockaddr_in servaddr, cliaddr; 
       
   
    //right now it does use udp for transmission, but it doesnt NEED to 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
       
    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(RELAY_PORT); 
       
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 ) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    socklen_t len;
    int n; 

    std::cout << "Server starting..." << std::endl;
    //multithread shouldnt be a need...yet

    while(true) {
        ClientPacket packet;
        len = sizeof(cliaddr);

        n = recvfrom(sockfd, &packet, buf_size,  
                    0, ( struct sockaddr *) &cliaddr, 
                    &len); 

        //TODO: add authentication for client
        bool res = dispatch_message(SockData(cliaddr, sockfd), packet);
        
        std::cerr << "sender host:port - " << sockaddr_to_hostport(cliaddr) << "\n" <<
        "real payload size " << n << "; claimed payload size " << packet.size << ", dispatch: " << res << std::endl;

    } 
    
    return 0; 
}
