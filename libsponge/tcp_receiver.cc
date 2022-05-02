#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

// in_window 判断 stream_index 是不是在当前的窗口中
bool TCPReceiver::in_window(const size_t &stream_index_left,const size_t &stream_index_right) const {
    size_t left_idx = _reassembler.get_should_write_idx();
    size_t right_idx = left_idx + window_size();
    if ( (stream_index_left >= left_idx && stream_index_left < right_idx) || 
    (stream_index_right >= left_idx && stream_index_right < right_idx)) {
        return true;
    } else {
        return false;
    }
}

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    if ((seg.header().syn && _isn_legal == true) || _reassembler.finish_reassembler())
    //如果出现了重复的syn or 已经结束了(eof都写入_output了),又收到了任何的报文,那么直接return false
        return false;
    if (seg.header().syn) {
        //如果现在收到的端是第一个段的话,设置isn
        _isn = seg.header().seqno;
        _isn_legal = true;
    }

    if (_isn_legal) {
        //只有当_isn有效的时候才进行接收
        //注意unwrap函数中的checkpoint根据lab2的pdf所说,应该使用should_write_idx.
        size_t stream_index = unwrap(seg.header().seqno, _isn, get_checkpoint()) - 1;  //见lab2.pdf同名字段

        if (!seg.header().syn) {
            //如果syn不存在,那么实际上本段的index应该对应实际位置载荷的-1的位置,
            //比如lab2.pdf中的图"cat"中的c的stream_index实际上比absolute seqno小1
            size_t stream_index_right = seg.payload().size()==0 ? stream_index:stream_index + seg.payload().size()-1;
            if (in_window(stream_index,stream_index_right)) {
                _reassembler.push_substring(seg.payload().copy(), stream_index, seg.header().fin);
                return true;
            } else {
                return false;
            }
        } else {
            //对于SYN=1的包来说,其stream_index为0
            stream_index++;
            _reassembler.push_substring(seg.payload().copy(), stream_index, seg.header().fin);
            return true;
        }
    }
    return false;
}

/*
弄清两个概念:
    absolute seqno:是对TCP流来说的,包括了SYN和FIN两个位置
    stream index:只是对载荷来说的,只是TCP用户字符串的index,不包括SYN和FIN.
    转换关系:absolute seqno = stream index + 1
*/
optional<WrappingInt32> TCPReceiver::ackno() const {
    //如果当前还没有收到一条带有syn的消息,那么返回null,否则返回ackno
    if (!_isn_legal)
        return nullopt;
    else if(_reassembler.finish_reassembler()){
        //注意如果全部输入完了,即eof也写入到_reassembler的_output中了,那么ack应该是当前的stream index+2
        //比如看lab2.pdf的"cat"写完之后,should_write_idx应该是3,而我们的ack应该是5
        return WrappingInt32(wrap(_reassembler.get_should_write_idx() + 2, _isn));
    }else{
        //absolute seqno = stream index + 1
        return WrappingInt32(wrap(_reassembler.get_should_write_idx() + 1, _isn));
    }     
}

size_t TCPReceiver::window_size() const { return _reassembler.get_window_size(); }
