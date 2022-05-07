#include "tcp_state.hh"

using namespace std;

bool TCPState::operator==(const TCPState &other) const {
    return _active == other._active and _linger_after_streams_finish == other._linger_after_streams_finish and
           _sender == other._sender and _receiver == other._receiver;
}

bool TCPState::operator!=(const TCPState &other) const { return not operator==(other); }

string TCPState::name() const {
    return "sender=`" + _sender + "`, receiver=`" + _receiver + "`, active=" + to_string(_active) +
           ", linger_after_streams_finish=" + to_string(_linger_after_streams_finish);
}

//根据tcp的若干state,构造对应的TCPState对象,并且赋值其中的_receiver和_sender两个state
TCPState::TCPState(const TCPState::State state) {
    switch (state) {
        case TCPState::State::LISTEN:
        //服务端的LISTEN状态,服务器创建socket,这时候还没有收到/发送任何包
            _receiver = TCPReceiverStateSummary::LISTEN;   	//等待客户端的syn
            _sender = TCPSenderStateSummary::CLOSED;		//还没发syn,所以是close            
            break;
        case TCPState::State::SYN_RCVD:
        //处于LISTEN的服务端收到了来自客户端的syn和seqno,发送ack和自己的seqno给客户端
            _receiver = TCPReceiverStateSummary::SYN_RECV;	//收到syn则变为正常接收状态
            _sender = TCPSenderStateSummary::SYN_SENT;		//sender已经发送了syn,但是接收方还没回ack
            break;
        case TCPState::State::SYN_SENT:
            _receiver = TCPReceiverStateSummary::LISTEN;	//recver还没收到任何的消息
            _sender = TCPSenderStateSummary::SYN_SENT;		//sender已经发送了syn
            break;
        case TCPState::State::ESTABLISHED:
            _receiver = TCPReceiverStateSummary::SYN_RECV;	//收到了另一方发来的syn,正常的接收
            _sender = TCPSenderStateSummary::SYN_ACKED;		//自己发的syn得到了ack,可以正常的发送
            break;
        case TCPState::State::CLOSE_WAIT:
            _receiver = TCPReceiverStateSummary::FIN_RECV;	//已经发送了fin,但是对于fin还没有收到对方的回复
            _sender = TCPSenderStateSummary::SYN_ACKED;		//正常接收
            _linger_after_streams_finish = false;
            break;
        case TCPState::State::LAST_ACK:
            _receiver = TCPReceiverStateSummary::FIN_RECV;	//已经收到了fin,停止接收
            _sender = TCPSenderStateSummary::FIN_SENT;		//已经send了fin,但是还没收到ack
            _linger_after_streams_finish = false;
            break;
        case TCPState::State::CLOSING:
            _receiver = TCPReceiverStateSummary::FIN_RECV;	//接收到了fin,停止接收
            _sender = TCPSenderStateSummary::FIN_SENT;		//已经send了fin,但是还没收到ack
            break;
        case TCPState::State::FIN_WAIT_1:
            _receiver = TCPReceiverStateSummary::SYN_RECV;	//正常的发送
            _sender = TCPSenderStateSummary::FIN_SENT;      //发送了fin,但是没有收到ack
            break;
        case TCPState::State::FIN_WAIT_2:
            _receiver = TCPReceiverStateSummary::SYN_RECV;		//没收到发送方的fin,所以正常接收
            _sender = TCPSenderStateSummary::FIN_ACKED;			//fin已经发送给对方,并且收到了ack
            break;
        case TCPState::State::TIME_WAIT:
            _receiver = TCPReceiverStateSummary::FIN_RECV;		//已经收到对方的fin
            _sender = TCPSenderStateSummary::FIN_ACKED;			//fin已经发送给对方,并且收到了ack
            break;
        case TCPState::State::RESET:
            _receiver = TCPReceiverStateSummary::ERROR;			//接收方error
            _sender = TCPSenderStateSummary::ERROR;				//发送方error
            _linger_after_streams_finish = false;
            _active = false;
            break;
        case TCPState::State::CLOSED:
            _receiver = TCPReceiverStateSummary::FIN_RECV;     //收到了fin,停止接收
            _sender = TCPSenderStateSummary::FIN_ACKED;        //已经发送了fin,并且收到了ack
            _linger_after_streams_finish = false;
            _active = false;
            break;
    }
}

TCPState::TCPState(const TCPSender &sender, const TCPReceiver &receiver, const bool active, const bool linger)
    : _sender(state_summary(sender))
    , _receiver(state_summary(receiver))
    , _active(active)
    , _linger_after_streams_finish(active ? linger : false) {}

string TCPState::state_summary(const TCPReceiver &receiver) {
    if (receiver.stream_out().error()) {
        return TCPReceiverStateSummary::ERROR;
    } else if (not receiver.ackno().has_value()) {
        return TCPReceiverStateSummary::LISTEN;
    } else if (receiver.stream_out().input_ended()) {
        return TCPReceiverStateSummary::FIN_RECV;
    } else {
        return TCPReceiverStateSummary::SYN_RECV;
    }
}

string TCPState::state_summary(const TCPSender &sender) {
    if (sender.stream_in().error()) {
        return TCPSenderStateSummary::ERROR;
    } else if (sender.next_seqno_absolute() == 0) {
        return TCPSenderStateSummary::CLOSED;
    } else if (sender.next_seqno_absolute() == sender.bytes_in_flight()) {
        return TCPSenderStateSummary::SYN_SENT;
    } else if (not sender.stream_in().eof()) {
        return TCPSenderStateSummary::SYN_ACKED;
    } else if (sender.next_seqno_absolute() < sender.stream_in().bytes_written() + 2) {
        return TCPSenderStateSummary::SYN_ACKED;
    } else if (sender.bytes_in_flight()) {
        return TCPSenderStateSummary::FIN_SENT;
    } else {
        return TCPSenderStateSummary::FIN_ACKED;
    }
}
