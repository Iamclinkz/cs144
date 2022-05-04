#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto(_initial_retransmission_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::do_send(const TCPSegment &seg) {
    size_t current_seqno = _next_seqno;
    size_t send_length = seg.length_in_sequence_space();
    _next_seqno += send_length;
    SegInfo seg_info = SegInfo(current_seqno + seg.length_in_sequence_space());
    if (!_timer.working()) {
        //如果当前没有timer,设置一手timer
        _timer.work(_rto);
    }
    _unack_seg.push(pair<SegInfo, TCPSegment>(seg_info, seg));
    _bytes_in_flight += send_length;
    _segments_out.push(seg);
}

void TCPSender::fill_window() {
    TCPSegment seg = TCPSegment();
    if (!_next_seqno) {
        //如果当前是第一个段,那么我们只需要发送一个带syn的tcpseg即可
        seg.header().syn = true;
        seg.header().seqno = wrap(0, _isn);
        do_send(seg);
        return;
    } else {
        if (!_window_size) {
            //如果window的长度为0,那么赋予其1,发送一个字节的内容(即使这个字节没啥意义)
            _window_size = 1;
        }

        if (_window_size <= _next_seqno - _max_recv_ackno) {
			//如果接收者的右窗口边界反而向左移动/当前的窗口大小一斤发完了,直接return
            return;
        }

        //计算接收方最多可以接受的大小，由于上面的判断,这里最小是1.
        size_t max_send_length_by_receiver_window = _window_size - (_next_seqno - _max_recv_ackno);
        //如果当前不是第一个包,那么我们需要根据当前的窗口等信息发送.
        while (!_sent_fin && max_send_length_by_receiver_window) {
            //如果我们的fin也已经发送出去了,直接不进入while循环

            //从当前窗口值和tcp最大载荷长度中选取最小的一个,然后从stream中读取一手.
            size_t read_len = min(max_send_length_by_receiver_window, TCPConfig::MAX_PAYLOAD_SIZE) - 1;

            //读取read_len - 1 的内容到read_content1中,read_len至少是0,所以要不就空读,要不就读read_len个字节
            string read_content1 = _stream.read(read_len);

            //再读取一个字节,如果当前没有出现eof,那么这个字节会加到content1后面,
            string read_content2 = _stream.read(1);
            if (!read_content2.size()) {
                if (_stream.eof()) {
                    //如果centent2空,并且当前出现了eof,那么我们放置一个eof,正常发送
                    seg.header().fin = true;
                    _sent_fin = true;
                } else if (!read_content1.size()) {
                    //如果centent2空,并且content1也空,并且没有eof,我们不发任何的包
                    break;
                }
            } else {
                read_content1 += read_content2;
            }
            seg.payload() = Buffer(move(read_content1));

            seg.header().seqno = wrap(_next_seqno, _isn);

            do_send(seg);
            max_send_length_by_receiver_window = _window_size - (_next_seqno - _max_recv_ackno);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t seqno = unwrap(ackno, _isn, _max_recv_ackno);
    if (seqno > _next_seqno) {
        //如果seqno确认的是当前还没发的字节,那么非法,返回false
        return false;
    } else if (seqno < _max_recv_ackno) {
        //如果已经确认过了,直接返回true,不做处理
        return true;
    } else {
        _max_recv_ackno = seqno;
        check_unack_seg(seqno);
        _window_size = window_size;
        fill_window();
        return true;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    bool overtime = _timer.refresh(ms_since_last_tick);
    if (overtime) {
        //如果超时,重新发送_unack_seg中的第一个TCPSeg
        do_resend();
    }
}

void TCPSender::do_resend() {
    if (!_unack_seg.empty()) {
        _segments_out.push(_unack_seg.front().second);
        _unack_seg.front().first.overtime_times++;
        _rto *= 2;
        _timer.work(_rto);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
	if(_unack_seg.empty()){
		return 0;
	}else{
		return _unack_seg.front().first.overtime_times;
	}
}

void TCPSender::send_empty_segment() {
    TCPSegment seg = {};
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

void TCPSender::check_unack_seg(const size_t ack_abs_seqno) {
    bool ack_ok = false;  //判断当前的ack是不是合法的ack,如果是合法的ack,设置rto
    while (!_unack_seg.empty()) {
        //逻辑:从_unack_seg的首部出发,向后遍历一手,如果当前的TCPSegment的最后一个字符也得到了确认,那么从_unack_seg中删除掉.
        if (ack_abs_seqno >= _unack_seg.front().first.absolute_seqno) {
            size_t ack_bytes_num = _unack_seg.front().second.length_in_sequence_space();
            _bytes_in_flight -= ack_bytes_num;
            _unack_seg.pop();
            ack_ok = true;
        } else {
            break;
        }
    }
    if (ack_ok) {
        _rto = _initial_retransmission_timeout;
        _timer.stop();
        if (!_unack_seg.empty()) {
            //根据lab3.pdf,如果当前还有未被确认的seg,那么重新定时定时器.
            _timer.work(_rto);
        }
    }
}
