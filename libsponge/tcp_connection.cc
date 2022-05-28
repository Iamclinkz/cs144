#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_active) {
        //如果当前并非活跃状态,直接返回
        return;
    }

    if (state() == TCPState::State::LISTEN) {
        //如果当前是Listen的状态
        if (seg.header().rst) {
            //如果来的第一个包有rst,我们应该将其忽略
            return;
        }
        if (seg.header().ack || !seg.header().syn) {    
            //如果第一个包里ack字段为true,或者没有syn,视为非法包,直接忽略即可
            return;
        }else{
            //当前的包是合法的,投喂到receiver里,赋值_isn
            _receiver.segment_received(seg);
            send_segs_in_sender(true);
            return;
        }
    } else if (state() == TCPState::State::SYN_SENT) {
        //如果当前是syn_sent的状态
        if (seg.header().rst) {
            if (_sender.check_ack_legal(seg.header().ackno)) {
                //如果包里有rst，检查ack是否合法，如果合法，直接shutdown，否则不做任何处理返回
                force_shutdown(false, seg.header().ackno);
            }
            return;
        }

        if (seg.header().syn) {
            if (seg.header().ack and !_sender.check_ack_legal(seg.header().ackno)) {
                //如果收到的段的ack字段存在,那么我们需要校验ackno字段是否合法,如果不合法,直接放弃包
                // force_shutdown(true, seg.header().ackno);
                return;
            } else {
                //如果收到的段的ack字段是合法的,并且syn字段为true,我们才可以把它当做正常的包
                _time_since_last_segment_received = 0;
                _sender.ack_received(seg.header().ackno, seg.header().win);
                _receiver.segment_received(seg);
                send_segs_in_sender(true);
                return;
            }
        } else {
            return;
        }
    }

    if (!_sender.ack_received(seg.header().ackno, seg.header().win) && !seg.header().rst) {
        //不知道为什么但是有个测试是测试这个的= =:如果接收方ack了一个发送方还没有发送的字节,
        //那么发送方应该发一个空段.
        send_segs_in_sender(true);
        return;
    }

    _time_since_last_segment_received = 0;

    if (_receiver.segment_received(seg)) {
        //如果该段有一部分是落在了窗口内,我们才能进行rst和keep-alive段的检测
        if (seg.header().rst) {
            //如果该段的rst字段被设置了,终止连接,不用发ack确认,直接set error即可
            force_shutdown(false, WrappingInt32(0));
            return;
        }

        //处理keep-alive段
        if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
            seg.header().seqno == _receiver.ackno().value() - 1 and !seg.header().rst) {
            send_segs_in_sender(true);
            return;
        }
    } else if (seg.header().rst) {
        return;
    }
    check_recv();
    send_segs_in_sender(seg.length_in_sequence_space());
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t ret = _sender.stream_in().write(data);
    send_segs_in_sender();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active) {
        return;
    }

    _time_since_last_segment_received += ms_since_last_tick;

    if (state() == TCPState::State::TIME_WAIT && _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _active = false;
        return;
    }

    _sender.tick(ms_since_last_tick);
    
    if(state() != TCPState::State::LISTEN){
        send_segs_in_sender(false);
    }

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        //如果超过了tcp的最大重复发送次数,关闭tcp连接
        force_shutdown(true, _sender.get_seqno());
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    send_segs_in_sender();
}

void TCPConnection::connect() { send_segs_in_sender(); }

TCPConnection::~TCPConnection() {
    force_shutdown(true, _sender.get_seqno());
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_segs_in_sender(bool send_empty) {
    _sender.fill_window();
    TCPSegment seg;
    if (_sender.segments_out().empty() && send_empty) {
        _sender.send_empty_segment();
    }
    while (!_sender.segments_out().empty()) {
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
    return;
}

void TCPConnection::send_single_seg(TCPSegment &seg, const WrappingInt32 &seqno) {
    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
        seg.header().win = _receiver.window_size();
    }
    seg.header().seqno = seqno;
    _segments_out.push(seg);
}

void TCPConnection::check_recv() {
    if (!_recv_peer_fin) {
        //如果还没有收到对方的fin,判断这次是否recv到了对方的fin
        if (_receiver.stream_out().input_ended()) {
            //如果收到了对方发送的fin
            _recv_peer_fin = true;
            if (!_sender.stream_in().eof()) {
                //如果是对面先发送的结束,而我们还没有结束 or 还没有
                //取走eof并且发送,那么我们属于是后结束的一方,不需要linger_after_streams_finish
                _linger_after_streams_finish = false;
            }
        }
    }

    if (_sender.sent_fin() && !_recv_my_fin_ack) {
        //如果当前已经发送了fin,但是对方还没有确认
        if (_sender.bytes_in_flight() == 0) {
            //如果收到了对方对于自己的fin的ack,那我们不需要回复
            _recv_my_fin_ack = true;
            if (_linger_after_streams_finish == false) {
                _active = false;
            }
        }
    }
}

void TCPConnection::force_shutdown(bool send_rst, const WrappingInt32 &seqno) {
    if (send_rst) {
        TCPSegment seg{};
        seg.header().rst = true;
        send_single_seg(seg, seqno);
    }

    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}