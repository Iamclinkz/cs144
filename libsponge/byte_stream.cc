#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : cap(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t ret = data.length()+this->buf.size()>this->cap ? this->cap-this->buf.size() : data.length();
    for (size_t i = 0; i < ret; i++)
        this->buf.push_back(data[i]);
    this->nwrite += ret;
    return ret;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ret;
    size_t count = len > this->buf.size() ? this->buf.size():len;
    for (size_t i = 0; i < count; i++)
        ret += this->buf[i];
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t count = len > this->buf.size() ? buf.size():len;
    this->nread += count;
    while(count--)
        this->buf.pop_front();
}

//仔细研究了一手才知道,这个api相当于是让发送方表明自己以后不发消息了.
/*
所以总体的逻辑是:
    发送方发送逻辑:
        while(buf有空闲 && 发送的内容不为空)
            发送
            发送的内容 -= 本次发送的内容
        input_ended()
    接收方接收逻辑:
        while(!eof())
            接收
*/
void ByteStream::end_input() {
    this->_eof = true;
}

//这个api是看发送方是不是从此不发送了.
bool ByteStream::input_ended() const {
    return this->_eof;
}

size_t ByteStream::buffer_size() const {
    return this->buf.size();
}

bool ByteStream::buffer_empty() const {
    return this->buf.size() == 0;
}

//这个api是是否读到了头,只有当前buf是空的,并且发送方再也不发送东西了,才相当于是大结束.
bool ByteStream::eof() const {
    return this->_eof && this->buffer_empty();
}

size_t ByteStream::bytes_written() const {
    return this->nwrite;
}

size_t ByteStream::bytes_read() const {
    return this->nread;
}

size_t ByteStream::remaining_capacity() const {
    return this->cap - this->buf.size();
}
