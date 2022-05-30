#ifndef SPONGE_LIBSPONGE_ETHERNET_HEADER_HH
#define SPONGE_LIBSPONGE_ETHERNET_HEADER_HH

#include "parser.hh"

#include <array>

//! Helper type for an Ethernet address (an array of six bytes)
using EthernetAddress = std::array<uint8_t, 6>;

//! Ethernet broadcast address (ff:ff:ff:ff:ff:ff)
constexpr EthernetAddress ETHERNET_BROADCAST = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

//! Printable representation of an EthernetAddress
std::string to_string(const EthernetAddress address);

//! \brief Ethernet frame header
struct EthernetHeader {
    static constexpr size_t LENGTH = 14;          //!< Ethernet header length in bytes
    static constexpr uint16_t TYPE_IPv4 = 0x800;  //!< Type number for [IPv4](\ref rfc::rfc791)
    static constexpr uint16_t TYPE_ARP = 0x806;   //!< Type number for [ARP](\ref rfc::rfc826)

    //! \name Ethernet header fields,14字节的以太网首部
    //!@{
    EthernetAddress dst;        //6byte的目的mac地址
    EthernetAddress src;        //6byte的源mac地址
    uint16_t type;              //2byte的帧类型,对于arp请求和arp应答为0x806,对于ipv4这个值为0x800
    //!@}

    //! Parse the Ethernet fields from the provided NetParser
    ParseResult parse(NetParser &p);

    //! Serialize the Ethernet fields to a string
    std::string serialize() const;

    //! Return a string containing a header in human-readable format
    std::string to_string() const;
};

//! \struct EthernetHeader
//! This struct can be used to parse an existing Ethernet header or to create a new one.

#endif  // SPONGE_LIBSPONGE_ETHERNET_HEADER_HH
