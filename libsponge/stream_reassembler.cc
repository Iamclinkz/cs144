#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;


Substring::Substring(const std::string &data, const uint64_t index) :_str(data),_hh(index),_tt(index + data.length()) {}

bool Substring::mergeable(const Substring &other) const {
    if (this->_hh > other._tt)
        return false;

    if (this->_tt < other._hh)
        return false;

    return true;
}

void Substring::doMerge(const Substring& other){
    string prefix;
    string suffix;
    if(this->_hh > other._hh){
        //应该由other作为头部
        size_t prefix_length = this->_hh - other._hh;
        prefix = other._str.substr(0, prefix_length);
        this->_hh = other._hh;
    }
    if(this->_tt < other._tt){
        size_t suffix_length = other._tt - this->_tt;
        suffix = other._str.substr(other._str.length() - suffix_length, other._str.length());
        this->_tt = other._tt;
    }
    this->_str = prefix + this->_str + suffix;
}

size_t StreamReassembler::get_window_size() const{
/*
运算逻辑:
    1.如果当前set中的第一个substring的_hh跟当前的_should_write_idx不同,那么说明我们当前的window_size应该是
    _cap大小,因为相当于希望收到的第一个byte还是_should_write_idx.
    2.如果当前set中的第一个substring的_hh跟当前的_should_write_idx相同,那么说明第一个set的内容肯定是按顺序的,
    只不过我们的byte_stream可能没空间写,所以暂时没写到里面去.所以当前的window_size应该是cap - 第一个set的
    substring的大小.
*/
    // if(_substring_set.empty() || _substring_set.begin()->_hh != _should_write_idx)
    //     return _capacity;
    // else
    //     return _capacity - _substring_set.begin()->strLen();
    return _capacity - stream_out().buffer_size();
}

StreamReassembler::StreamReassembler(const size_t capacity) : _free_space(capacity),_capacity(capacity),_output(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if(eof){
        _have_eof = true;
        _eof_pos = data.length() + index;
        if(!data.length())
            doWrite();
    }
    uint64_t tmp_index = index;
    if(data.length()+tmp_index <= _should_write_idx){
        //检查data,如果data已经完全的被输出到_output中了,那么直接狗带
        return;
    }
    string tmp_data = data;

    //如果data有一部分已经被写入_output了,那么截断这部分
    if(tmp_index < _should_write_idx){
        tmp_data = tmp_data.substr(_should_write_idx-tmp_index,tmp_data.length());
        tmp_index = _should_write_idx;
    }
    if(tmp_index >= _should_write_idx+_capacity){
        //如果tmp_data的tmp_index超过了我们的容量,比如cap是10,我们当前_should_write_idx是1,说明我们希望1~10号char,
        //所以如果出现了tmp_data的tmp_index=11,tmp_data.length()是10,但是1+10 <= 11,那么我们只能将tmp_data丢掉,反之我们一定能容纳一部分
        return;
    }

    if(_free_space < tmp_data.length() || index + tmp_data.length() > _should_write_idx + _capacity){
        //如果不能完全的放下,那么需要截断一部分的字符串
        //从lab2发现的bug= =...例如cap是2,我们希望放"abc",先放入"bc"的话其实只应该存储"b"
        size_t cap_len = _capacity-(index-_should_write_idx); 
        size_t miner = min(cap_len,_free_space);
        tmp_data = tmp_data.substr(0,miner);
    }
    //如果可以放下,并且当前tmp_data的eof为true的话,说明将tmp_data写到输入流就算写完了,所以设置eof
    auto new_ss = Substring(tmp_data,tmp_index);
    for(auto iter = _substring_set.begin() ; iter != _substring_set.end();){
        if(new_ss.mergeable(*iter)){
            //如果可以和当前的存在的substring融合,那么融合一手
            new_ss.doMerge(*iter);
            _current_len -= iter->strLen();
            _free_space += iter->strLen();
            iter = _substring_set.erase(iter);
        }else{
            iter++;
        }
    }
    _substring_set.insert(new_ss);
    _free_space -= new_ss.strLen();
    _current_len += new_ss.strLen();
    doWrite();
}

void StreamReassembler::doWrite(){
    if (!_substring_set.empty() && _should_write_idx == _substring_set.begin()->_hh){
        //如果当前需要写入的字符的idx和当前的substring_set中存储的substring的第一个字符的idx相等,那么进行write
        auto iter = _substring_set.begin();
        size_t nw = _output.write(_substring_set.begin()->_str);
        if(nw == iter->_str.length()){
            //如果把第一个substring全写了,那么删除掉第一个substring
            _substring_set.erase(iter);
        }else{
            //如果把第一个substring的一部分写了,set不支持直接更改,需要先删掉再插入
            Substring first = Substring(iter->_str.substr(nw, iter->_str.length()), iter->_hh + nw);
            _substring_set.erase(iter);
            _substring_set.insert(first);
        }
        _current_len -= nw;
        _should_write_idx += nw;
        _free_space += nw;
    }
    if(_have_eof && _should_write_idx == _eof_pos){
        _output.end_input();
    }
}


size_t StreamReassembler::unassembled_bytes() const { return this->_current_len; }

bool StreamReassembler::empty() const { return _current_len; }
