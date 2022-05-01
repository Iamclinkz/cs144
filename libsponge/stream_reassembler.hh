#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <set>
#include <string>

class Substring {
  public:
    Substring() = default;
    std::string _str;  //实际存储substring
    size_t _hh = 0;    //本substring的首个字节位于大字符串的位置
    size_t _tt = 0;    //本substring的最后一个字节的后面一个字节位于大字符串的位置
    size_t strLen() const { return _tt - _hh; }
    Substring(const std::string &data, const uint64_t index);

    // mergeable 查看两个substring是否可以合并,比如首个
    bool mergeable(const Substring &other) const;

    // doMerge 将另一个Substring融合到本substring中,返回融合之后增加的长度
    void doMerge(const Substring &other);

    bool operator<(const Substring &other) const { return this->_hh < other._hh; }
};

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    std::set<Substring> _substring_set = {};
    size_t _current_len = 0;       //当前的所有substring的长度.
    size_t _free_space;            //当前还剩下多少长度
    size_t _should_write_idx = 0;  //当前应该写入的第一个字节
    size_t _capacity;              //!< The maximum number of bytes
    ByteStream _output;            //!< The reassembled in-order byte stream
    bool _have_eof = false;
    size_t _eof_pos = -1;
    void doWrite();

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receives a substring and writes any newly contiguous bytes into the stream.
    //!
    //! If accepting all the data would overflow the `capacity` of this
    //! `StreamReassembler`, then only the part of the data that fits will be
    //! accepted. If the substring is only partially accepted, then the `eof`
    //! will be disregarded.
    //!
    //! \param data the string being added
    //! \param index the index of the first byte in `data`
    //! \param eof whether or not this segment ends with the end of the stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been submitted twice, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
