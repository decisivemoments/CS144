#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    const auto arp_entry = _arp_table.find(next_hop_ip);
    if(arp_entry == _arp_table.end()){
        // ARP entry not found
        const auto arp_request = _arp_requests_sent.find(next_hop_ip);
        if(arp_request == _arp_requests_sent.end()){
            // ARP request not sent
            // send ARP request
            ARPMessage arp_request_message;
            arp_request_message.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request_message.sender_ethernet_address = _ethernet_address;
            arp_request_message.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request_message.target_ip_address = next_hop_ip;
            EthernetFrame frame;
            frame.header() = {
                ETHERNET_BROADCAST,
                _ethernet_address,
                EthernetHeader::TYPE_ARP
            };
            frame.payload() = arp_request_message.serialize();
            _frames_out.push(frame);
            _arp_requests_sent[next_hop_ip] = 0;
        }
        _wait_for_reply_data.push_back(std::make_pair(next_hop, dgram));
    }else {
        // ARP entry found
        EthernetFrame frame;
        EthernetHeader &header = frame.header();
        header.dst = arp_entry->second.ethernet_address;
        header.src = _ethernet_address;
        header.type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST){
        // not for me
        return nullopt;
    }
    if(frame.header().type == EthernetHeader::TYPE_IPv4){
        // IPv4 datagram
        InternetDatagram dgram;
        if(dgram.parse(frame.payload()) == ParseResult::NoError){
            return dgram;
        }else{
            return nullopt;
        }
    }else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_message;
        if(arp_message.parse(frame.payload()) == ParseResult::NoError){
            if(arp_message.opcode == ARPMessage::OPCODE_REQUEST && arp_message. target_ip_address == _ip_address.ipv4_numeric()){
                // ARP request for me
                ARPMessage arp_reply;
                arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                arp_reply.sender_ethernet_address = _ethernet_address;
                arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
                arp_reply.target_ethernet_address = arp_message.sender_ethernet_address;
                arp_reply.target_ip_address = arp_message.sender_ip_address;
                EthernetFrame frame_reply;
                frame_reply.header() = {
                    arp_message.sender_ethernet_address,
                    _ethernet_address,
                    EthernetHeader::TYPE_ARP
                };
                frame_reply.payload() = arp_reply.serialize();
                _frames_out.push(frame_reply);
                
            }
            _arp_table[arp_message.sender_ip_address] = {
                arp_message.sender_ethernet_address,
                0
            };
            if(_arp_requests_sent.erase(arp_message.sender_ip_address) > 0){
                for(auto it = _wait_for_reply_data.begin(); it != _wait_for_reply_data.end();){
                    if(it->first.ipv4_numeric() == arp_message.sender_ip_address){
                        send_datagram(it->second, it->first);
                        it = _wait_for_reply_data.erase(it);
                    }else{
                        it++;
                    }
                }
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for(auto it = _arp_table.begin(); it != _arp_table.end();){
        it->second.time_since_last_seen += ms_since_last_tick;
        if(it->second.time_since_last_seen >= ARP_TTL){
           it = _arp_table.erase(it);
        }else{
            it++;
        }
    }
    for(auto it = _arp_requests_sent.begin(); it != _arp_requests_sent.end();){
        it->second += ms_since_last_tick;
        if(it->second >= ARP_REQUEST_TIMEOUT){
            ARPMessage arp_request_message;
            arp_request_message.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request_message.sender_ethernet_address = _ethernet_address;
            arp_request_message.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request_message.target_ip_address = it->first;
            arp_request_message.target_ethernet_address = {};
            EthernetFrame frame;
            frame.header() = {
                ETHERNET_BROADCAST,
                _ethernet_address,
                EthernetHeader::TYPE_ARP
            };
            frame.payload() = arp_request_message.serialize();
            _frames_out.push(frame);
            it->second = 0;
        }else{
            it++;
        }
    }
}
