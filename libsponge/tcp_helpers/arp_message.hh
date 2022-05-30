#ifndef SPONGE_LIBSPONGE_ARP_MESSAGE_HH
#define SPONGE_LIBSPONGE_ARP_MESSAGE_HH

#include "ethernet_header.hh"
#include "ipv4_header.hh"

using EthernetAddress = std::array<uint8_t, 6>;

//! \brief [ARP](\ref rfc::rfc826) message
struct ARPMessage {
    static constexpr size_t LENGTH = 28;          //!< ARP message length in bytes
    static constexpr uint16_t TYPE_ETHERNET = 1;  //!< ARP type for Ethernet/Wi-Fi as link-layer protocol
    static constexpr uint16_t OPCODE_REQUEST = 1;
    static constexpr uint16_t OPCODE_REPLY = 2;

    //! \name ARPheader fields 28字节的arp报文格式(见https://blog.csdn.net/lm409/article/details/80299823)
    //arp包同ip包一样,作为12byte的以太网帧的载荷
    //!@{
    //硬件类型,以太网类型为1(同时还有wifi类型等)
    uint16_t hardware_type = TYPE_ETHERNET;              //!< Type of the link-layer protocol (generally Ethernet/Wi-Fi)
    //要通过哪种协议的地址做映射.这里选用ipv4地址->mac地址
    uint16_t protocol_type = EthernetHeader::TYPE_IPv4;  //!< Type of the Internet-layer protocol (generally IPv4)
    uint8_t hardware_address_size = sizeof(EthernetHeader::src);    //mac地址长度
    uint8_t protocol_address_size = sizeof(IPv4Header::src);        //ip地址长度
    uint16_t opcode{};  //!< Request or reply                       //是arp请求(1)还是arp应答(2)

    EthernetAddress sender_ethernet_address{};                      //发送端arp请求 or 应答的硬件地址(同字段2,为mac地址)
    uint32_t sender_ip_address{};                                   //发送arp请求 or 应答的协议地址(此处为ip地址)

    EthernetAddress target_ethernet_address{};                      //目的硬件地址(此处为mac地址)
    uint32_t target_ip_address{};                                   //目的协议地址(此处为ip地址)
    //!@}

    //! Parse the ARP message from a string
    ParseResult parse(const Buffer buffer);

    //! Serialize the ARP message to a string
    std::string serialize() const;

    //! Return a string containing the ARP message in human-readable format
    std::string to_string() const;

    //! Is this type of ARP message supported by the parser?
    bool supported() const;
};

//! \struct ARPMessage
//! This struct can be used to parse an existing ARP message or to create a new one.

#endif  // SPONGE_LIBSPONGE_ETHERNET_HEADER_HH
