#ifndef SPONGE_LIBSPONGE_TCP_STATE
#define SPONGE_LIBSPONGE_TCP_STATE

#include "tcp_receiver.hh"
#include "tcp_sender.hh"

#include <string>

//! \brief Summary of a TCPConnection's internal state
//!
//! Most TCP implementations have a global per-connection state
//! machine, as described in the [TCP](\ref rfc::rfc793)
//! specification. Sponge is a bit different: we have factored the
//! connection into two independent parts (the sender and the
//! receiver). The TCPSender and TCPReceiver maintain their interval
//! state variables independently (e.g. next_seqno, number of bytes in
//! flight, or whether each stream has ended). There is no notion of a
//! discrete state machine or much overarching state outside the
//! sender and receiver. To test that Sponge follows the TCP spec, we
//! use this class to compare the "official" states with Sponge's
//! sender/receiver states and two variables that belong to the
//! overarching TCPConnection object.
class TCPState {
  //上述大意是:tcp有自己的状态机和标准的状态表示例如"SYN_RCVD"等.而我们自己的lab中有我们自己的
  //sender和recver的自定义状态机以及自定义状态表示(即下面的TCPReceiverStateSummary和TCPReceiverStateSummary)
  //我们可以通过tcp_sender.cc中的TCPState的构造函数,把两者进行转换.
  private:
    std::string _sender{};
    std::string _receiver{};
    bool _active{true};
    bool _linger_after_streams_finish{true};

  public:
    bool operator==(const TCPState &other) const;
    bool operator!=(const TCPState &other) const;

    //! \brief Official state names from the [TCP](\ref rfc::rfc793) specification
    enum class State {
        LISTEN = 0,   //!< Listening for a peer to connect
        SYN_RCVD,     //!< Got the peer's SYN
        SYN_SENT,     //!< Sent a SYN to initiate a connection
        ESTABLISHED,  //!< Three-way handshake complete
        CLOSE_WAIT,   //!< Remote side has sent a FIN, connection is half-open
        LAST_ACK,     //!< Local side sent a FIN from CLOSE_WAIT, waiting for ACK
        FIN_WAIT_1,   //!< Sent a FIN to the remote side, not yet ACK'd
        FIN_WAIT_2,   //!< Received an ACK for previously-sent FIN
        CLOSING,      //!< Received a FIN just after we sent one
        TIME_WAIT,    //!< Both sides have sent FIN and ACK'd, waiting for 2 MSL
        CLOSED,       //!< A connection that has terminated normally
        RESET,        //!< A connection that terminated abnormally
    };

    //! \brief Summarize the TCPState in a string
    std::string name() const;

    //! \brief Construct a TCPState given a sender, a receiver, and the TCPConnection's active and linger bits
    TCPState(const TCPSender &sender, const TCPReceiver &receiver, const bool active, const bool linger);

    //! \brief Construct a TCPState that corresponds to one of the "official" TCP state names
    TCPState(const TCPState::State state);

    //! \brief Summarize the state of a TCPReceiver in a string
    static std::string state_summary(const TCPReceiver &receiver);

    //! \brief Summarize the state of a TCPSender in a string
    static std::string state_summary(const TCPSender &receiver);
};

namespace TCPReceiverStateSummary {
//tcp流出错状态,stream_out().error() == true,让上层感知到tcp出错
const std::string ERROR = "error (connection was reset)";
//本tcp receiver作为服务端,等待客户端的连接,recv.ackno()应该为空,表示当前还没收到来自
//客户端的包,从而也就没有ackno().
const std::string LISTEN = "waiting for stream to begin (listening for SYN)";
//正在正常的接收. recv.ackno()不为空,并且没有收到落在窗口内的fin信息
const std::string SYN_RECV = "stream started";
//已经收到了来自窗口内的fin信息,从而已经执行了 stream_out().input_ended(),通知上层本recv接收结束
const std::string FIN_RECV = "stream finished";
}  // namespace TCPReceiverStateSummary

namespace TCPSenderStateSummary {
//tcp流出错状态,stream_in().error() == true,让上层感知到tcp出错
const std::string ERROR = "error (connection was reset)";
//虽然已经初始化了sender对象,但是还没有进行sync的发送.
//即next_seqno_abs()为0,表示下一个应该发送的abs_seqno应该是0,即syn标志位
const std::string CLOSED = "waiting for stream to begin (no SYN sent)";
//作为客户端,syn已经发送了,但是还没有来自服务器的ack.
//即next_seqno_abs() == bytes_in_flight() == 1,表示当前已经发送,并且只发送了syn
const std::string SYN_SENT = "stream started but nothing acknowledged";
//已经收到了来自服务器端的ack,本机发->服务器收 的流通道正常
const std::string SYN_ACKED = "stream ongoing";
//已经发送了fin,但是还没有收到服务器的对于fin的ack
//即stream_in().eof() == true,并且next_seqno_abs() == 应用层发送的byte数目+2
//并且bytes_in_flight() > 0
const std::string FIN_SENT = "stream finished (FIN sent) but not fully acknowledged";
//服务器已经ack了自己的fin.本机发->服务器收的流通道已经关闭
//相比上一个状态,bytes_in_flight() = 0
const std::string FIN_ACKED = "stream finished and fully acknowledged";
}  // namespace TCPSenderStateSummary

#endif  // SPONGE_LIBSPONGE_TCP_STATE
