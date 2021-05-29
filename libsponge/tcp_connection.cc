#include "tcp_connection.hh"

#include <iostream>

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

//! \brief check the seg's flags
//! \note if RST, set error to inbound and outbound streams, destoryed the connection
//! \note give it to TCPReciever, which can inspect with seqno, SYN, playload and FIN
//! \note if ACK, tell TCPSender the seg's ackno and window_size
//! \note if seg occupied any seqno (not an full empty seg), make sure to send AT LEAST ONE segment

void TCPConnection::segment_received(const TCPSegment &seg) { 
    if(not _timer.is_on())
        _timer.start();
    _timer.set_zero();
    if(seg.header().rst){
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        
        return;
    }
    _receiver.segment_received(seg);
    if(seg.header().ack){
        //TODO: tell the TCPSender ackno and window_size
    }
    if(!seg.length_in_sequence_space())
        return;
    //TODO: make sure to send at least one segment
    if(_sender.segments_out().empty()) 
        _sender.send_empty_segment();
    send_segment();
}

bool TCPConnection::active() const { return _active; }


//! \brief tick will be called periodly by operate system 
//! \note tell TCPSender about the passage of time
//! \note if consecutive retransmissions is too much, destory the connect
//! \note else if connect is closed, end it cleanly
//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _sender.tick(ms_since_last_tick);
    
    if(_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS){
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        TCPConnection::~TCPConnection();
    }
    if(!_timer.is_on())
        return;
    _timer.time_passed(ms_since_last_tick);

    if(_receiver.stream_out().eof() 
    and unassembled_bytes() == 0 
    and _sender.stream_in().eof() 
    and _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 
    and bytes_in_flight() == 0
    and time_since_last_segment_received() > ((_cfg.rt_timeout * 5)<<1)){
        _active = false;
    }

}

//! \name "Input" interface for the writer
//!@{

//! \note send a SYN by \param _sender
void TCPConnection::connect() {
    _sender.fill_window();
    send_segment();
}

//! \note  write data into \param _sender.stream_in and send it
size_t TCPConnection::write(const string &data) {
    if(data.size() == 0)
        return 0;
    size_t write_len = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segment();
    return write_len;
}

//! \note return \param _sender.stream_in remaining capacity
size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

//! \note set \param _sender.stream_in end input
void TCPConnection::end_input_stream() { _sender.stream_in().end_input();}
//!@}

//! \name Accessors used for testing
//!@{

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _timer.time(); }

//!@}

void TCPConnection::send_segment(){ 
    // TODO: make sure send out every segment can be sent
    
    // when _sender.segments_out() is not empty, send it (push it into _segments_out and pop)
    if(_sender.segments_out().empty())
        return;
    TCPSegment seg = _sender.segments_out().front();
    // before send a segment, set the info from the TCPReceiver (ackno and window_size)

    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
    }
    _sender.segments_out().front().header().win = _receiver.window_size()> UINT16_MAX? static_cast<uint16_t>(_receiver.window_size() ): UINT16_MAX;
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();
    return;
}
TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();
            while(_sender.segments_out().front().length_in_sequence_space() !=0)
                _sender.segments_out().pop();
            _sender.segments_out().front().header().rst = true;
            _segments_out.push(_sender.segments_out().front());

        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
