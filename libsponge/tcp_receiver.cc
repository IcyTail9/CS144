#include "tcp_receiver.hh"
#include <iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

void TCPReceiver::segment_received(const TCPSegment &seg) {
    //TCPHeader header = seg.header();

    // receive the first SYN segment
    if (seg.header().syn) {
        // ignore repeat syn segment
        if(_isn)
            return;
        //_isn = std::make_optional<WrappingInt32>(header.seqno.raw_value());
        //isn = std::make_optional<WrappingInt32>(seg.header().seqno.raw_value());   
        _isn = seg.header().seqno;
    }

    // if a link setup, receive data
    if (_isn and not stream_out().input_ended()) {
        // unwrap a seqno to a absolute number
        uint64_t abs_num = unwrap(seg.header().seqno, _isn.value(), _checkpoint);
        // reset checkpoint
        _checkpoint = abs_num;

        // write the data into resembler
        _reassembler.push_substring(seg.payload().copy(), seg.header().syn ? 0 : abs_num - 1, seg.header().fin);
    }
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    size_t shift = 1;
    if (stream_out().input_ended() && _reassembler.unassembled_bytes() == 0)
        shift = 2;
    //if(_isn and not stream_out().input_ended())
    if(_isn)
        //return std::make_optional<WrappingInt32>(_reassembler.first_unassemble() + shift + _isn.value().raw_value());
        return wrap(_reassembler.first_unassemble() + shift, _isn.value());
    //if(stream_out().input_ended())
    //    return wrap(_reassembler.first_unassemble()+2,_isn.value());
    return std::nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }