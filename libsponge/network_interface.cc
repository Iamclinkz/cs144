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
void DUMMY_CODE(Targs &&.../* unused */) {}

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
    map<uint32_t, EthernetAddrWrapper>::iterator it = _ip_to_mac.find(next_hop_ip);
    if (it == _ip_to_mac.end()) {
        //如果没有找到该ip地址对应的mac地址的话,发送req
        send_arp_req(next_hop_ip, dgram);
    } else {
        //构建一个以太网帧
        EthernetFrame eth_frame{};
        eth_frame.header().dst = (*it).second.eth_addr;
        eth_frame.header().src = _ethernet_address;
        eth_frame.header().type = EthernetHeader::TYPE_IPv4;
        eth_frame.payload() = dgram.serialize();
        _frames_out.push(eth_frame);
    }
}

void NetworkInterface::check_arp_cache(const uint32_t &ip_addr, const EthernetAddress &eth_addr) {
    //先插入一手
    _ip_to_mac.insert(pair<uint32_t, EthernetAddrWrapper>(ip_addr, EthernetAddrWrapper(eth_addr)));
    auto it = _unknow_mac_frame_map.find(ip_addr);
    if (it != _unknow_mac_frame_map.end()) {
        //如果这个是我们当前要查找的ip,将等待该ip的mac的所有报文进行发送
        while (!(*it).second.unsend_ip_datagram_list.empty()) {
            EthernetFrame eth_frame{};
            eth_frame.header().dst = eth_addr;
            eth_frame.header().src = _ethernet_address;
            eth_frame.header().type = EthernetHeader::TYPE_IPv4;
            eth_frame.payload() = (*it).second.unsend_ip_datagram_list.front().serialize();
            _frames_out.push(eth_frame);
            (*it).second.unsend_ip_datagram_list.pop();
        }
        _unknow_mac_frame_map.erase(it);
        if (_arp_request_send_list.front() == ip_addr) {
            //查看是不是队头,如果是队头,需要弹出,然后发送下一个
            _arp_request_send_list.pop();
            while (!_arp_request_send_list.empty()) {
                auto it2 = _unknow_mac_frame_map.find(_arp_request_send_list.front());
                if (it2 != _unknow_mac_frame_map.end()) {
                    _frames_out.push((*it2).second.unknow_mac_addr_frame);
                } else {
                    _arp_request_send_list.pop();
                }
            }
        }
    }
}
//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (mac_addr_equal(frame.header().dst, ETHERNET_BROADCAST)) {
        //如果该帧是广播
        ARPMessage arp_msg{};
        if (arp_msg.parse(frame.payload()) == ParseResult::NoError) {
            //如果译码没问题,我们记录一手sender的ip地址和mac地址的映射关系
            check_arp_cache(arp_msg.sender_ip_address, arp_msg.sender_ethernet_address);
            if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST &&
                arp_msg.target_ip_address == _ip_address.ipv4_numeric()) {
                //如果是arp请求包,并且请求的是自己的mac地址,那么我们回复一手
                ARPMessage arp_resp{};
                arp_resp.opcode = ARPMessage::OPCODE_REPLY;
                arp_resp.sender_ip_address = _ip_address.ipv4_numeric();
                arp_resp.sender_ethernet_address = _ethernet_address;
                arp_resp.target_ip_address = arp_msg.sender_ip_address;
                arp_resp.target_ethernet_address = arp_msg.sender_ethernet_address;

                EthernetFrame arp_resp_eth_frame{};
                arp_resp_eth_frame.header().dst = arp_msg.sender_ethernet_address;
                arp_resp_eth_frame.header().src = _ethernet_address;
                arp_resp_eth_frame.header().type = EthernetHeader::TYPE_ARP;
                arp_resp_eth_frame.payload() = arp_resp.serialize();
                _frames_out.push(arp_resp_eth_frame);
            }
        }
    } else if (mac_addr_equal(frame.header().dst, _ethernet_address)) {
        //如果该帧是给自己的
        if (frame.header().type == EthernetHeader::TYPE_IPv4) {
            //如果是ipv4类型,进行parse,如果有错则返回null,如果没错返回InternetDatagram
            InternetDatagram dgram{};
            if (dgram.parse(frame.payload()) == ParseResult::NoError)
                return dgram;
            return nullopt;
        } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
            //如果是arp回复包
            ARPMessage arp_msg{};
            if (arp_msg.parse(frame.payload()) == ParseResult::NoError && arp_msg.opcode == ARPMessage::OPCODE_REPLY) {
                //如果译码成功,并且是arp的reply,进行插入
                check_arp_cache(arp_msg.sender_ip_address, arp_msg.sender_ethernet_address);
            }
        }
    }
    //如果不是给自己的也不是广播的,那么狗带
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    bool popped = false;
    for (auto it = _ip_to_mac.begin(); it != _ip_to_mac.end();) {
        (*it).second.recv_time += ms_since_last_tick;
        if ((*it).second.recv_time > 30000) {
            //如果该表项超时
            _ip_to_mac.erase(it++);
        } else {
            it++;
        }
    }

    while (!_arp_request_send_list.empty()) {
        uint32_t first_ip = _arp_request_send_list.front();
        auto it = _unknow_mac_frame_map.find(first_ip);
        if (it == _unknow_mac_frame_map.end()) {
            //如果没有找到,说明已经知道了,不用发送了
            _arp_request_send_list.pop();
            popped = true;
        } else {
            if (popped) {
                //如果队头的已经被弹出了,那么应该重新发送一手下一个请求
                _frames_out.push((*it).second.unknow_mac_addr_frame);
            } else {
                //如果队头的没有被弹出,那么队头的计时器增加一手
                (*it).second.sent_time += ms_since_last_tick;
                if ((*it).second.sent_time > 5000) {
                    (*it).second.sent_time = 0;
                    _frames_out.push((*it).second.unknow_mac_addr_frame);
                }
            }
            return;
        }
    }
}

void NetworkInterface::send_arp_req(uint32_t request_ip, const InternetDatagram &unknow_mac_ip_datagram) {
    auto it = _unknow_mac_frame_map.find(request_ip);
    if (it == _unknow_mac_frame_map.end()) {
        //如果没有发送该ip的arp请求包，那么构造一个链路层帧，发送一下
        ARPMessage arp_msg{};
        arp_msg.sender_ip_address = _ip_address.ipv4_numeric();
        arp_msg.sender_ethernet_address = _ethernet_address;
        arp_msg.target_ip_address = request_ip;
        arp_msg.target_ethernet_address = {0, 0, 0, 0, 0, 0};
        arp_msg.opcode = ARPMessage::OPCODE_REQUEST;

        //以太网帧
        EthernetFrame arp_req_eth_frame{};
        arp_req_eth_frame.header().dst = ETHERNET_BROADCAST;
        arp_req_eth_frame.header().src = _ethernet_address;
        arp_req_eth_frame.header().type = EthernetHeader::TYPE_ARP;
        arp_req_eth_frame.payload() = arp_msg.serialize();

        //构造一个UnknowMacAddrFrame,放到_unknow_mac_frame_map中
        auto umf = UnknowMacAddrFrame(request_ip, arp_req_eth_frame);
        umf.unsend_ip_datagram_list.push(unknow_mac_ip_datagram);
        _unknow_mac_frame_map.insert(pair<uint32_t, UnknowMacAddrFrame>(request_ip, umf));
        //放到发送队列中
        _arp_request_send_list.push(request_ip);
        //如果位于发送队列的头部(发送队列之前为空),则立刻发送
        if (_arp_request_send_list.front() == request_ip) {
            _frames_out.push(arp_req_eth_frame);
        }
    } else {
        (*it).second.unsend_ip_datagram_list.push(unknow_mac_ip_datagram);
    }
}

bool NetworkInterface::mac_addr_equal(const EthernetAddress &a, const EthernetAddress &b) {
    for (size_t i = 0; i < 6; i++)
        if (a[i] != b[i])
            return false;
    return true;
}
