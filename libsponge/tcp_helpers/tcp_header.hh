#ifndef SPONGE_LIBSPONGE_TCP_HEADER_HH
#define SPONGE_LIBSPONGE_TCP_HEADER_HH

#include "parser.hh"
#include "wrapping_integers.hh"

//! \brief [TCP](\ref rfc::rfc793) segment header
//! \note TCP options are not supported
struct TCPHeader {
    static constexpr size_t LENGTH = 20;  //!< [TCP](\ref rfc::rfc793) header length, not including options

    //! \struct TCPHeader
    //! ~~~{.txt}
    //!   0                   1                   2                   3
    //!   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |          Source Port          |       Destination Port        |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                        Sequence Number                        |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                    Acknowledgment Number                      |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |  Data |           |U|A|P|R|S|F|                               |
    //!  | Offset| Reserved  |R|C|S|S|Y|I|            Window             |
    //!  |       |           |G|K|H|T|N|N|                               |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |           Checksum            |         Urgent Pointer        |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                    Options                    |    Padding    |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //!  |                             data                              |
    //!  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //! ~~~

    //! \name TCP Header fields
    //!@{
    uint16_t sport = 0;         //!< source port
    uint16_t dport = 0;         //!< destination port
    WrappingInt32 seqno{0};     //!< sequence number
    WrappingInt32 ackno{0};     //!< ack number
    uint8_t doff = LENGTH / 4;  //!< data offset
    bool urg = false;           //!< urgent flag
    bool ack = false;           //!< ack flag
    bool psh = false;           //!< push flag
    bool rst = false;           //!< rst flag
    bool syn = false;           //!< syn flag         如果设置了syn,那么载荷+1 byte
    bool fin = false;           //!< fin flag         如果设置了fin,那么载荷+1 byte
    uint16_t win = 0;           //!< window size
    uint16_t cksum = 0;         //!< checksum
    uint16_t uptr = 0;          //!< urgent pointer
    //!@}

    //! Parse the TCP fields from the provided NetParser
    //即用一个含有string的buf,生成一个NetParser对象,然后作为本函数的参数,用于给TCPHeader的各个字段赋值
    //相当于解码
    ParseResult parse(NetParser &p);

    //! Serialize the TCP fields
    //将本TCPHeader转换成可以发送的string格式,相当于编码.
    std::string serialize() const;

    //! Return a string containing a header in human-readable format
    //相当于golang的String(),打印一手头部信息
    std::string to_string() const;

    //! Return a string containing a human-readable summary of the header
    //打印标志位等信息.
    std::string summary() const;

    bool operator==(const TCPHeader &other) const;
};

#endif  // SPONGE_LIBSPONGE_TCP_HEADER_HH
