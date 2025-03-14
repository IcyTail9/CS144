#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>
#include <iostream>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

// TODO: write a TCPSegTimer class to modify the code
using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto(retx_timeout) {}

//! \note returns bytes those sent yet not acked
//! \note _next_seqno equals total bytes sent
//! \note _last_ackno equals total bytes acked
uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _last_ackno; }

//! \note Things to do with a TCPSegment and internal structures
//! \note 1.decide SYN and FIN flag 
//! \note 2.decide the playload size
//! \note 3.read from \param _stream
//! \note 4.sent out the TCPSegment to \param _segments_out and store in \param _segments_outstanding
//! \note Things to do with Timer
//! \note When not sending an empty Seg, if \param timer euals 0, set a expire time to \param timer

void TCPSender::fill_window() {
    TCPSegment seg;
    size_t win = static_cast<size_t>(_window_size == 0 ? 1 : _window_size);
    // "CLOSED"
    if(next_seqno_absolute() == 0){
        seg.header().syn = true;
        send_segment(seg);
        return;
    }
    // "SYN_SENT"
    if(next_seqno_absolute() == bytes_in_flight()){
        return;        
    }
    // "SYN_ACKED" (when SYN is acked, can send data)
    if (not stream_in().eof()) {
        while (not _stream.buffer_empty() and win > bytes_in_flight()) {
            size_t _send_len = min({win - bytes_in_flight(),
                                    _stream.buffer_size(),
                                    static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE)});
            seg.payload() = _stream.read(_send_len);
            if (_stream.eof() and bytes_in_flight() + _send_len < win and
                next_seqno_absolute() < _stream.bytes_written() + 2)
                seg.header().fin = true;
            send_segment(seg);
        }
        return;
    }
    // "SYN_ACKED"
    if (bytes_in_flight() < win and next_seqno_absolute() < stream_in().bytes_written() + 2) {
    // to SENT A FIN:
        seg.header().fin = true;
        send_segment(seg);
        return;
    }
    // // "FIN_SENT"
    // if (bytes_in_flight() > 0)
    //     return;
    // // "FIN_ACKED"
    // if (bytes_in_flight() == 0) {
    //     return;
    // }
    // "ERROR"
    if (_stream.error())
        return;
}

//! \param ackno The peer's ackno (acknowledgment number)
//! \param window_size The peer's advertised window size
//! \note Things to do with \param _last_ackno: set \param[in] ackno to it
//! \note Things to do with \param _segments_outstanding: check & remove the TCPSegment acked

//! \note If an outstanding TCPSegment is acked, do as follows:
//! \param _intial_retransmission_timeout: reset to initial value
//! \param _consecutive_retrans_time: reset to zero
//! \param _timer: if \param _segments_outstanding not empty, set \param _rto to it
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    uint64_t new_ackno = unwrap(ackno, _isn, _next_seqno);
    if(_last_ackno > new_ackno or new_ackno > _next_seqno )
        return;
    // refresh peer's window size
    _window_size = window_size;
    // // count for repeat ackno(for quick retransmit)
    // if(new_ackno == _last_ackno)
    //     ++_consecutive_ack_receive_times;
    // else
    //     _consecutive_ack_receive_times = 0;
    // refresh ackno
    _last_ackno = new_ackno;

    // remove acked segments from _segments_outstanding
    if(not _segments_outstanding.empty()){
        TCPSegment seg = _segments_outstanding.front();
        while (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= _last_ackno) {
            // reset overtime settings
            _rto = _initial_retransmission_timeout;
            _timer.start();
            _consecutive_retrans_times = 0;

            _segments_outstanding.pop();
            if (_segments_outstanding.empty())
                break;
            seg = _segments_outstanding.front();
        }
    }

    if(_segments_outstanding.empty())
        _timer.close();
    
    fill_window();
    return;
}



//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
//! if \param _timer > 0, means \param _segments_outstanding !empty(), 
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer.is_on()) 
        return;
    _timer.time_passed(ms_since_last_tick);
    if (_timer.time() >= _rto and not _segments_outstanding.empty()) {
        // retrans the first outstanding segment
        segments_out().push(_segments_outstanding.front());
        if (_window_size > 0 or _segments_outstanding.front().header().syn) {
            ++_consecutive_retrans_times;
            _rto <<= 1;
        }
        _timer.start();
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retrans_times; }

void TCPSender::send_empty_segment() {

    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}

void TCPSender::send_segment(TCPSegment& seg){

    seg.header().seqno = next_seqno();
    _next_seqno += seg.length_in_sequence_space();

    _segments_out.push(seg);
    _segments_outstanding.push(seg);

    if (not _timer.is_on())
        _timer.start();
}
