
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


using uint64 = unsigned long long;
using byte = unsigned char;

enum MESSAGE_TYPE : byte {
    GET_PEERLIST = 0,
    RELAY_PACKET = 1
};



constexpr size_t buf_size = 65536;
struct UDPPacket {
    MESSAGE_TYPE type;
};
//udp port
constexpr int RELAY_PORT = 16969;
inline static char packet_buffer[buf_size]; 

std::string addr_to_string(const in_addr& addr) {
    char client_str[128];
    inet_ntop(AF_INET, &(addr), client_str, INET_ADDRSTRLEN);
    return client_str;

}

std::string sockaddr_to_hostport(const sockaddr_in& addr) {
    return addr_to_string(addr.sin_addr) + ":" + std::to_string(ntohs(addr.sin_port));
}

std::mutex peers_lock;

std::unordered_map<uint64, sockaddr_in> peers;

void add_peer(uint64 id, const sockaddr_in& peer_sock) {
    std::lock_guard lock(peers_lock);
    //there really is no point keeping
    peers.erase(id);
    peers.emplace(id, peer_sock);
}

const sockaddr_in* find_peer(uint64 id) {
    std::lock_guard lock(peers_lock);
    auto it = peers.find(id);
    if(it == peers.end())
        return nullptr;
    return &(it->second);
}




int main() { 
    int sockfd; 
    
    struct sockaddr_in servaddr, cliaddr; 
       
    // Creating socket file descriptor 
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
        len = sizeof(cliaddr);

        n = recvfrom(sockfd, &packet_buffer, buf_size,  
                    0, ( struct sockaddr *) &cliaddr, 
                    &len); 
        byte* buf_pointer = (byte*)&packet_buffer;
        MESSAGE_TYPE msg_type = (MESSAGE_TYPE)*buf_pointer;
        buf_pointer += sizeof(MESSAGE_TYPE);
        //TODO: unify id size
        int dst_uid = *reinterpret_cast<int*>(buf_pointer);
        buf_pointer += sizeof(int);

        int src_uid = *reinterpret_cast<int*>(buf_pointer);
        buf_pointer += sizeof(int);
        
        //TODO: add authentication for client

        
        std::cout << "sender uid - " << src_uid << " host:port - " << sockaddr_to_hostport(cliaddr) << " payload size " << n << std::endl;
        //add to list of peers
        add_peer(src_uid, cliaddr);
        //drop packet if not relaying
        if(msg_type != RELAY_PACKET)
            continue;
        
        //find requested peer
        const sockaddr_in* send_addr = find_peer(dst_uid);
        //drop if didnt find peer
        if(send_addr == nullptr)
            continue;
        std::cout << "forwarding to uid: " << dst_uid << " host:port " << sockaddr_to_hostport(*send_addr) << std::endl;
        //forward packet, send everything except dst + mode(redundant)
        int to_send_n = n - 5;
        char* pbuf = (char*)&packet_buffer;
        pbuf += 5;
        
        int sent_n = sendto(sockfd, pbuf, to_send_n, 0, (sockaddr*)send_addr, sizeof(*send_addr));
    } 
    
    return 0; 
}
