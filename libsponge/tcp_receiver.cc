#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    // ignore repeat syn segment
    if (_syn && header.syn)
        return;
    // setup a link
    if (header.syn) {
        _syn = true;
        _isn = header.seqno.raw_value();
    }

    // if a link setup, receive data
    if (_syn) {
        // unwrap a seqno to a absolute number
        uint64_t abs_num = unwrap(header.seqno, WrappingInt32(_isn), _checkpoint);
        // reset checkpoint
        _checkpoint = abs_num;
        // endup a link
        if (header.fin)
            _fin = true;

        // write the data into resembler
        _reassembler.push_substring(seg.payload().copy(), header.syn ? 0 : abs_num - 1, _fin);
    }
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    size_t shift = 1;
    if (_fin && _reassembler.unassembled_bytes() == 0)
        shift = 2;
    if(_syn)
        return wrap(_reassembler.first_unassemble() + shift, WrappingInt32(_isn));
    return {};
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }