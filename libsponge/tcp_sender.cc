#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return {}; }

//! \note Things to do with a TCPSegment and internal structures
//! \note 1.decide SYN and FIN flag 
//! \note 2.decide the playload size
//! \note 3.read from \param _stream
//! \note 4.sent out the TCPSegment to \param _segments_out and store in \param _segments_outstanding
//! \note Things to do with Timer
//! \note When not sending an empty Seg, if \param timer euals 0, set a expire time to \param timer

void TCPSender::fill_window() {
    // start a link, send a SYN
    if(!_syn){
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = _isn;
        _segments_out.push(seg);
        _segments_outstanding.push(seg);
        ++_next_seqno;
        return;
    }
    if(_stream.input_ended() && _stream.buffer_empty()){
        TCPSegment seg;
        seg.header().fin = true;
        seg.header().seqno = next_seqno();
        _segments_out.push(seg);
        _segments_outstanding.push(seg);
        ++_next_seqno;
        return;
    }
    if(_stream.buffer_empty()){
        send_empty_segment();
        return;
    }
    // when SYN is acked, can send data
    else if(_segments_outstanding.empty() || _segments_outstanding.front().header().syn == false){
        uint16_t _send_len;

        if(_window_size > bytes_in_flight()){
            _send_len = min(_window_size - bytes_in_flight(),_stream.buffer_size(),TCPConfig::MAX_PAYLOAD_SIZE);
        }
        else _send_len = 1;

        TCPSegment seg;

        seg.header().seqno = next_seqno();
        seg.payload().str() = _stream.read(_send_len);
        
        _segments_out.push(seg);
        _segments_outstanding.push(seg);

        //_next_seqno += _send_len;
        _bytes_in_flight += _send_len;


    }
    // when window_size(actual) = window_size - bytes_in_flights > 0 && !_stream.empty(), send data

    // when window_size == 0, send a 1 bit TCPSegent to detect new window

    // when 

    // when no data left, send a FIN



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
    _window_size = window_size;

    uint64_t _ackno_in_stream = unwrap(ackno, _isn, _next_seqno);    
    TCPSegment seg = _segments_outstanding.front();

    while (!_segments_outstanding.empty() && unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= _ackno_in_stream) {
            
            _bytes_in_flight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();

            //! @reset the timer
            _initial_retransmission_timeout = TCPConfig::TIMEOUT_DFLT;
            _timer = _initial_retransmission_timeout;
            _consecutive_retrans_times = 0;
            //! @

            _next_seqno += seg.length_in_sequence_space();
            _ackno_in_stream = unwrap(ackno, _isn, _next_seqno);

            seg = _segments_outstanding.front();
    }

    if (!_bytes_in_flight)
        _timer = 0;
    // Note that test code will call it again.
    fill_window();
}



//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
//! if \param _timer > 0, means \param _segments_outstanding !empty(), 
void TCPSender::tick(const size_t ms_since_last_tick) {  }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}
