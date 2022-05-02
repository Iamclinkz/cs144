#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return WrappingInt32(static_cast<int32_t>(n) + isn.raw_value()); }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    /*
    目的:
        找到一个k,使得checkpoint和 k * 2^32 + n_real的距离最近.
    运算逻辑:
        设 checkpoint_header 为checkpoint & 0xFFFFFFFF00000000ul(即只保留checkpoint的高32位)
        查看case1: checkpoint_header+n_real
            case2: checkpoint_header+n_real - 0x100000000ul
            case3: checkpoint_header+n_real + 0x100000000ul
        这三种情况,哪个和checkpoint离得最近.
    例子:
        假设要找一个k,使得checkpoint 和 k*10000 + n_real 的距离最近
        case1最优的情况:例如checkpoint = 1,n_real = 2 -> case1 = 0+1 = 1,距离checkpoint最近
        case2最优的情况:例如checkpoint = 9999,n_real = 1 -> case3 = case1 + 10000 = 0+1+10000=10001,距离checkpoint最近
        case3最优的情况:例如checkpoint = 10001,n_real = 9999 -> case3 = case1-10000 =
    10000+9999-10000=9999,距离checkpoint最近
    */
    uint32_t n_real = n.raw_value() - isn.raw_value();
    uint64_t case1 = (checkpoint & 0xFFFFFFFF00000000ul) + n_real;
    uint64_t ret = case1;
    if (abs(int64_t(case1 + 0x100000000ul - checkpoint)) < abs(int64_t(case1 - checkpoint)))
        ret = case1 + 0x100000000ul;
    if (case1 >= 0x100000000ul && abs(int64_t(case1 - 0x100000000ul - checkpoint)) < abs(int64_t(ret - checkpoint)))
        ret = case1 - 0x100000000ul;
    return ret;
}
