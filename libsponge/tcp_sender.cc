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
//! \note _stream.bytes_read() equals total bytes_sent_out
//! \note next_seqno_absolute() equals total bytes_acked
uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _last_ack_seqno; }

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
    if(next_seqno_absolute() > 0
    and next_seqno_absolute() == bytes_in_flight()){
        cout<<" go into SYN_SENT func"<<endl;
        return;        
    }
    
    // when SYN is acked, can send data
    // "SYN_ACKED"
    if (next_seqno_absolute() > bytes_in_flight() and not stream_in().eof()) {
        
        while (not stream_in().buffer_empty() and win > bytes_in_flight()) {
            size_t _send_len = min({win - bytes_in_flight(),
                                    _stream.buffer_size(),
                                    static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE)});
            seg.payload() = _stream.read(_send_len);
            if (stream_in().eof() and bytes_in_flight() + _send_len < win and
                next_seqno_absolute() < stream_in().bytes_written() + 2)
                seg.header().fin = true;
            send_segment(seg);
        }
        return;
    }
    // "SYN_ACKED"
    if (stream_in().eof() and bytes_in_flight() < win and next_seqno_absolute() < stream_in().bytes_written() + 2) {
    //SENT_FIN:
        seg.header().fin = true;
        send_segment(seg);
        return;
    }

    // "FIN_SENT"
    if (stream_in().eof() and next_seqno_absolute() == stream_in().bytes_written() + 2 and bytes_in_flight() > 0)
        return;
    // "FIN_ACKED"
    if (stream_in().eof() and next_seqno_absolute() == stream_in().bytes_written() + 2 and bytes_in_flight() == 0) {
        _timer.close();
        return;
    }
    // "ERROR"
    if (_stream.error())
        return;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \note Things to do with \param _next_seqno: set \param[in] ackno to it
//! \note Things to do with \param _segments_outstanding: check & remove the TCPSegment acked

//! \note If an outstanding TCPSegment is acked, do as follows:
//! \param _intial_retransmission_timeout: reset to initial value
//! \param _consecutive_retrans_time: reset to zero
//! \param _timer: if \param _segments_outstanding not empty, set \param _rto to it
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    if(_last_ack_seqno > unwrap(ackno, _isn, _next_seqno))
        return;

    _window_size = window_size;
    _last_ack_seqno = unwrap(ackno, _isn, _next_seqno);

    if(not _segments_outstanding.empty()){
        TCPSegment seg = _segments_outstanding.front();
        while (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= _last_ack_seqno) {
            // reset overtime settings
            // TODO: reduce repeat setting 0
            _rto = _initial_retransmission_timeout;
            _timer.start();
            _consecutive_retrans_times = 0;

            _segments_outstanding.pop();
            if (_segments_outstanding.empty())
                break;
            seg = _segments_outstanding.front();
        }
    }
    if(_segments_outstanding.empty()){
        _timer.close();
    }
    //_next_seqno = _ackno_in_stream;
    fill_window();
}



//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
//! if \param _timer > 0, means \param _segments_outstanding !empty(), 
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer.is_on()) 
        return;
    _timer.time_passed(ms_since_last_tick);
    if (_timer.time() >= _rto and not _segments_outstanding.empty()) {
        // retrans i
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
    segments_out().push(seg);
}

void TCPSender::send_segment(TCPSegment& seg){
    seg.header().seqno = next_seqno();
    _next_seqno += seg.length_in_sequence_space();
    segments_out().push(seg);
    _segments_outstanding.push(seg);
    if (not _timer.is_on())
        _timer.start();
}
