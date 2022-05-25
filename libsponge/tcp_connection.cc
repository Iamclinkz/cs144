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

    //判断是否落在了合法区间之内
    if (!_sender.ack_received(seg.header().ackno, seg.header().win)) {
        return;
    }

    _time_since_last_segment_received = 0;

    if (seg.header().rst) {
        //如果该段的rst字段被设置了,终止连接,不用发ack确认,直接set error即可
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        return;
    }

    //处理keep-alive段
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        return;
    }

    _receiver.segment_received(seg);
    send_segs_in_sender();
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
    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        //如果超过了tcp的最大重复发送次数,关闭tcp连接
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    send_segs_in_sender();
}

void TCPConnection::connect() { send_segs_in_sender(); }

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_segs_in_sender() {
    bool sent = false;
    _sender.fill_window();
    TCPSegment seg;
    while (!_sender.segments_out().empty()) {
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
        sent = true;
    }

    if (!sent) {
        _sender.send_empty_segment();
    }
    return;
}

void TCPConnection::send_single_seg(TCPSegment &seg) { 
    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
        seg.header().win = _receiver.window_size();
    }
	_segments_out.push(seg);
}

void TCPConnection::clean_shutdown() {
	if(_receiver.stream_out().input_ended() and _sender.stream_in().input_ended()){
		//如果是对面先发送的结束,那么我们不需要linger_after_streams_finish
		_linger_after_streams_finish = false;
	}
	
}

void TCPConnection::force_shutdown() {
	TCPSegment seg {};
	seg.header().rst = true;
	send_single_seg(seg);
	_sender.stream_in().set_error();
	_receiver.stream_out().set_error();
	_active = false;
}