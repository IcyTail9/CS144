#include "tcp_connection.hh"

#include <iostream>

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

//! \brief check the seg's flags
//! \note if RST, set error to inbound and outbound streams, destoryed the connection
//! \note give it to TCPReciever, which can inspect with seqno, SYN, playload and FIN
//! \note if ACK, tell TCPSender the seg's ackno and window_size
//! \note if seg occupied any seqno i.e. not a pure ACK, make sure to send AT LEAST ONE segment

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (not _active)
        return;
    _timer.set_zero();
    // if RST
    if (seg.header().rst) {
        shutdown();
        return;
    }
    
    // give it to _receiver
    _receiver.segment_received(seg);


    // if ACK, tell the TCPSender ackno and window_size
    if (seg.header().ack)
        _sender.ack_received(seg.header().ackno, seg.header().win);
    
    // do not ACK for ACK
    // bug: cannot reply a window detect seg in a half-close connection(in FIN ACK, waiting for peer's fin)
    // solution: reply it in tick() method
    if (seg.length_in_sequence_space() == 0)
        return;
    
    // send seg
    _sender.fill_window();
    // if a seg receive, must send a seg
    if (_sender.segments_out().empty())
        _sender.send_empty_segment();
    pop_all_seg_in_sender();
}

bool TCPConnection::active() const { return _active; }

//! \brief tick will be called periodly by operate system 
//! \note tell TCPSender about the passage of time
//! \note if consecutive retransmissions is too much, destory the connect
//! \note else if connect is closed, end it cleanly
//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if(not _active or not _timer.is_on())
        return;

    _sender.tick(ms_since_last_tick);
    _timer.time_passed(ms_since_last_tick);

    // Timeout, unclean shutdown
    if(_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS){
        shutdown();
        send_rst();
        return;
    }
    // declare a not zero window size to peer, need to refresh win
    if(not _window_not_zero and _receiver.window_size() > 0 and _sender.segments_out().empty()){
        _sender.send_empty_segment();
    }
    // _sender.tick() may cause retransmission
    pop_all_seg_in_sender();
}

//! \name "Input" interface for the writer
//!@{

//! \note send a SYN by \param _sender
void TCPConnection::connect() {
    _sender.fill_window();
    pop_all_seg_in_sender();
}

//! \note  write data into \param _sender.stream_in and send it
size_t TCPConnection::write(const string &data) {
    size_t write_len = _sender.stream_in().write(data);
    _sender.fill_window();
    pop_all_seg_in_sender();
    return write_len;
}

//! \note return \param _sender.stream_in remaining capacity
size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

//! \note set \param _sender.stream_in end input
void TCPConnection::end_input_stream() { 
    _sender.stream_in().end_input();
    _sender.fill_window();
    pop_all_seg_in_sender();
}
//!@}

//! \name Accessors used for testing
//!@{

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _timer.time(); }

//!@}

void TCPConnection::pop_all_seg_in_sender(){ 
    while (not _sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        // before send a segment, set the info from the TCPReceiver (ackno and window_size)
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win =
            _receiver.window_size() > UINT16_MAX ? UINT16_MAX: static_cast<uint16_t>(_receiver.window_size());

        // if declare a zero window, mark it
        _window_not_zero = static_cast<bool>(seg.header().win);

        _segments_out.push(seg);
    }
    clean_end();
}
void TCPConnection::send_rst() {
    _sender.send_empty_segment();

    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();

    seg.header().rst = true;

    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
    }

    _segments_out.push(seg);

    clean_end();
}
void TCPConnection::clean_end() {
    // Prereq #1: inbound stream has been fully assembled and has ended i.e. receiver has received a FIN
    if (unassembled_bytes() == 0 and _receiver.stream_out().eof()) {
        // Prereq #2: outbound stream has been ended and sent a FIN (FIN SENT)
        // Option B: if Prereq #1 is True while Prereq #2 is False, then NO NEED lingering
        if (not(_sender.stream_in().eof() and
                _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2)) {
            _linger_after_streams_finish = false;
            return;
        }
        // Prereq #3: outbound stream has been fully acked (FIN ACK) and timeout
        if (_sender.bytes_in_flight() == 0) {
            // Option A: if need lingering, set NOT ACTIVE when TIMEOUT
            // Option B: else directly set NOT ACTIVE
            if (time_since_last_segment_received() >= 10 * _cfg.rt_timeout or not _linger_after_streams_finish)
                _active = false;
        }
    }
}
void TCPConnection::shutdown(){
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
}
TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            shutdown();
            send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

