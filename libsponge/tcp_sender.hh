#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//定时器,如果在工作,表示的是当前_unack_seg中的位于队头的pair有没有超时.
class TCPTimer {
  private:
    bool _working = false;  //当前是否在工作
    long _time = 0;       //当前距离超时的剩余时间
  public:
    TCPTimer() = default;

    // work 开始以设定的时间(set_time)的计时工作
    void work(size_t set_time) {
        if (_working) {
            return;
        } else {
            _working = true;
            _time = set_time;
        }
    }

    // refresh 由tick()函数调用,更新内部的时间,如果当前正在工作,并且已经超时,返回true,否则返回false,表示没有超时.
    bool refresh(const size_t passed_time) {
        if (!_working)
            //如果没有在工作,直接返回false表示没有超时
            return false;
        else {
            if ((_time -= passed_time) <= 0) {
                //如果在工作,并且已经超时,那么返回true表示已经超时.
                _working = false;
                _time = 0;
                return true;
            } else {
                //如果在工作但是没有超时,返回false
                return false;
            }
        }
    }

    // stop 停止当前的计时器的计时工作
    void stop() {
        _working = false;
        _time = 0;
    }

    // working 是否正在工作
    bool working() { return _working; }
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    uint16_t _window_size = 1;
    //超时使用,从_unack_seg中取出第一个tcpseg,并且重新发送一手
    void do_resend();

    //正常使用,发送一个tcpseg,并且进行timer的检查,放到_unack_seg中等工作.
    void do_send(const TCPSegment &);

    //当前的超时时间
    size_t _rto;
    size_t _max_recv_ackno = 0;
    //因为queue不支持遍历,所以这个字段用于记录当前queue中有多少个有效载荷
    size_t _bytes_in_flight = 0;
    // size_t _first_unack_abs_seqno = 0;
    //用于存放_unack_seg中的段的信息
    struct SegInfo {
        size_t absolute_seqno = 0;  //本段最后一个字符的下一个字符的在absolute seqno中的位置
        size_t overtime_times = 0;  //本段超时次数
        SegInfo(size_t seqno) : absolute_seqno(seqno) {}
    };
    //保存 <本段的SegInfo , 还没有收到确认的段>组成的pair
    //如果发生了超时,则将队头的第一个发送出去.注意pair.first应该是在队列中有序的.
    std::queue<std::pair<SegInfo, TCPSegment>> _unack_seg{};
    //收到ack后,检查_unack_seg,查看其中得到确认的段,将其删除
    void check_unack_seg(const size_t seqno);
    TCPTimer _timer = {};
    //是否已经发送了fin位
    bool _sent_fin = false;

  public:
  //得到一个合法的seqno,如果发送队列不是空的,将发送队列中的首个段的seqno返回,否则返回seqno
    WrappingInt32 get_seqno();
    // lab4 当前是否已经发送了fin位
    bool sent_fin() { return _sent_fin; }
    //只是检测是不是合法
    bool check_ack_legal(const WrappingInt32 ackno);
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    bool ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
