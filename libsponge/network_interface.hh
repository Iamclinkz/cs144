#ifndef SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH
#define SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH

#include "ethernet_frame.hh"
#include "tcp_over_ip.hh"
#include "tun.hh"

#include <map>
#include <optional>
#include <queue>
//! \brief A "network interface" that connects IP (the internet layer, or network layer)
//! with Ethernet (the network access layer, or link layer).

//! This module is the lowest layer of a TCP/IP stack
//! (connecting IP with the lower-layer network protocol,
//! e.g. Ethernet). But the same module is also used repeatedly
//! as part of a router: a router generally has many network
//! interfaces, and the router's job is to route Internet datagrams
//! between the different interfaces.

//! The network interface translates datagrams (coming from the
//! "customer," e.g. a TCP/IP stack or router) into Ethernet
//! frames. To fill in the Ethernet destination address, it looks up
//! the Ethernet address of the next IP hop of each datagram, making
//! requests with the [Address Resolution Protocol](\ref rfc::rfc826).
//! In the opposite direction, the network interface accepts Ethernet
//! frames, checks if they are intended for it, and if so, processes
//! the the payload depending on its type. If it's an IPv4 datagram,
//! the network interface passes it up the stack. If it's an ARP
//! request or reply, the network interface processes the frame
//! and learns or replies as necessary.
//相当于是进行网络层<->数据链路层的接口层.其功能为:
// 1.接收到上层发来的ip数据报,选择(如果不存在那么使用arp查找)合适的mac地址,并且封装成链路层帧并且发给下一跳
// 2.接收到包之后,根据包的类型进行反应:如果是arp广播报文,那么放弃 or 回复
//  如果是传给自己的链路层帧,那么根据报文内的mac地址,决定是否转拆解成ip报文,然后传给上层.
class NetworkInterface {
  private:
    //! Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
    //也就是本机的mac地址.
    EthernetAddress _ethernet_address;

    //! IP (known as internet-layer or network-layer) address of the interface
    //也就是本机的ip地址
    Address _ip_address;

    //! outbound queue of Ethernet frames that the NetworkInterface wants sent
    std::queue<EthernetFrame> _frames_out{};

    // lab5
    //存放ip地址到mac地址的映射
    struct EthernetAddrWrapper {
        EthernetAddress eth_addr{};
        size_t recv_time = 0;
        EthernetAddrWrapper(const EthernetAddress &addr) { eth_addr = addr; }
    };

    std::map<uint32_t, EthernetAddrWrapper> _ip_to_mac = {};

    struct UnknowMacAddrFrame {
        //已经发送了多长时间
        size_t sent_time = 0;
        // ip地址是多少
        uint32_t ip_addr = 0;
        //存放了arp请求报文,方便如果本次无响应,下次继续发送
        EthernetFrame unknow_mac_addr_frame{};
        //存放了由于不知道本ip地址对应的mac地址,所以没法发送的ip数据报
        std::queue<InternetDatagram> unsend_ip_datagram_list{};
        UnknowMacAddrFrame(const uint32_t &unknow_mac_ip_addr, const EthernetFrame &frame) {
            ip_addr = unknow_mac_ip_addr;
            unknow_mac_addr_frame = frame;
        }
    };
    //在map中表示已经发送了,key为为止mac的ip,value为该ip对应的UnknowMacAddrFrame结构
    std::map<uint32_t, UnknowMacAddrFrame> _unknow_mac_frame_map{};
    // arp发送的顺序,规定每个机器每一时刻只能有一个arp请求在传播
    std::queue<uint32_t> _arp_request_send_list{};
    //如果该未知mac的ip已经发送了arp包,那么直接将 unknow_mac_frame
    //插入到UnknowMacAddrFrame中即可,如果没有发送,则新建一个
    // UnknowMacAddrFrames,并且将这个UnknowMacAddrFrames插入到_unknow_mac_frame_map中
    void send_arp_req(uint32_t request_ip, const InternetDatagram &unknow_mac_ip_datagram);
    bool mac_addr_equal(const EthernetAddress &a, const EthernetAddress &b);
    void check_arp_cache(const uint32_t &ip_addr, const EthernetAddress &eth_addr);

  public:
    //! \brief Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer) addresses
    //通过mac地址和ip地址构造一个NetworkInterface类
    NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address);

    //! \brief Access queue of Ethernet frames awaiting transmission
    std::queue<EthernetFrame> &frames_out() { return _frames_out; }

    //! \brief Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination address).

    //! Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next hop
    //! ("Sending" is accomplished by pushing the frame onto the frames_out queue.)
    void send_datagram(const InternetDatagram &dgram, const Address &next_hop);

    //! \brief Receives an Ethernet frame and responds appropriately.

    //! If type is IPv4, returns the datagram.
    //! If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
    //! If type is ARP reply, learn a mapping from the "target" fields.
    std::optional<InternetDatagram> recv_frame(const EthernetFrame &frame);

    //! \brief Called periodically when time elapses
    void tick(const size_t ms_since_last_tick);
};

#endif  // SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH
