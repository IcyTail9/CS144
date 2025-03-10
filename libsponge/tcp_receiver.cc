#include "tcp_receiver.hh"
#include <iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // first SYN recv
    if (seg.header().syn) {
        // ignore repeat syn segment
        if(_remote_isn)
            return; 
        _remote_isn = seg.header().seqno;
    }

    // if a link setup, receive data
    if (_remote_isn and not stream_out().input_ended()) {
        // unwrap a seqno to a absolute number
        uint64_t abs_num = unwrap(seg.header().seqno, _remote_isn.value(), _checkpoint);
        // reset checkpoint
        _checkpoint = abs_num;
        // write the data into resembler, if a FIN recv, set the stream input ended
        _reassembler.push_substring(seg.payload().copy(), seg.header().syn ? 0 : abs_num - 1, seg.header().fin);
    }
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    size_t shift = 1;
    // "FIN_SENT"
    if (stream_out().input_ended() && _reassembler.unassembled_bytes() == 0)
        shift = 2;
    // “SYN_RECV"
    if(_remote_isn.has_value())
        return wrap(_reassembler.first_unassemble() + shift, _remote_isn.value());
    return std::nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
