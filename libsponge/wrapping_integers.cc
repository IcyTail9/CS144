#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.



using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    
    return WrappingInt32{static_cast<uint32_t>(n) + isn.raw_value()};
}

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
    //uint32_t checkpoint_in_seqno = static_cast<uint_32>(checkpoint) + isn.raw_value();
    //int32_t offset = n.raw_value() - checkpoint_in_seqno;

    //! \param offset equals the distance between n & checkpoint, while offset <= UINT32_MAX
    //! \param offset > 0, means n > checkpoint_in_seqno
    //! \param offset < 0, means n < checkpoint_in_seqno
    int32_t offset = n.raw_value() - (isn.raw_value() + static_cast<uint32_t>(checkpoint)); 

    //! \param n's stream_indices form eauals checkpoint + offset
    int64_t res = checkpoint + offset;
    //! \param checkpoint can be near zero,
    return  res >= 0 ? res : res + UINT32_MAX + 1;
}