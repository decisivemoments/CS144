#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , RTO(retx_timeout)
    , _stream(capacity)
    , timer(0){}

uint64_t TCPSender::bytes_in_flight() const { 
    return _bytes_in_flight;
 }

void TCPSender::fill_window() {
    // bytes has send
    uint16_t bytes_has_send = 0;
    // bytes should sned
    uint16_t bytes_should_send = 0;
    if(_window_size == 0){
        bytes_should_send = 1;
    }else{
        bytes_should_send = _window_size;
    }
    if(bytes_should_send < _bytes_in_flight){
        return;
    }
    bytes_should_send -= bytes_in_flight();
    while(bytes_has_send < bytes_should_send){
        TCPSegment seg;
        seg.header().seqno = wrap(_next_seqno, _isn);
        seg.header().syn = _next_seqno == 0;
        uint64_t bytes_this_time = bytes_should_send - bytes_has_send - (_next_seqno == 0);

        if(_stream.buffer_size() <= TCPConfig::MAX_PAYLOAD_SIZE){
            if(bytes_this_time > _stream.buffer_size()){
                bytes_this_time = _stream.buffer_size();
                if(_stream.input_ended() && _fin_sent == false){
                    seg.header().fin = true;
                    _fin_sent = true;
                }    
            }
        }else if(_stream.buffer_size() > TCPConfig::MAX_PAYLOAD_SIZE){
            bytes_this_time = bytes_this_time < TCPConfig::MAX_PAYLOAD_SIZE ? bytes_this_time : TCPConfig::MAX_PAYLOAD_SIZE;
        }

        if(bytes_this_time == 0 && seg.header().fin == false && seg.header().syn == false){
            break;
        }

        seg.payload() = _stream.read(bytes_this_time);
        bytes_has_send += seg.length_in_sequence_space();
        _next_seqno += seg.length_in_sequence_space();
        _segments_out.push(seg);
        _segments_in_flight.push(seg);
        _bytes_in_flight += seg.length_in_sequence_space();
        if(timer.started() == false){
            timer.reset(RTO);
        }
        if(seg.header().fin == true){
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    uint64_t ackno_unwrap = unwrap(ackno, _isn, _next_seqno);
    if(ackno_unwrap > _next_seqno){
        return;
    }
    _window_size = window_size;
    if(_ackno == ackno_unwrap){
        return;
    }else{
        _ackno = ackno_unwrap;
    }
    while(!_segments_in_flight.empty()){
        TCPSegment seg = _segments_in_flight.front();
        uint64_t seqno_unwrap = unwrap(seg.header().seqno, _isn, _next_seqno);
        if(seqno_unwrap + seg.length_in_sequence_space() <= ackno_unwrap){
            _segments_in_flight.pop();
            _bytes_in_flight -= seg.length_in_sequence_space();
        }else{
            break;
        }
    }
    RTO = _initial_retransmission_timeout;
    if(!_segments_in_flight.empty()){
        timer.reset(RTO);
    }else {
        timer.close();
    }
    _consecutive_retransmissions = 0;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if(timer.started() == false){
        return;
    }
    // printf("tick:%lu\n", ms_since_last_tick);
    timer.tick(ms_since_last_tick);
    if(timer.expired()){
        // printf("expired\n");
        _segments_out.push(_segments_in_flight.front());
        if(_window_size){
            _consecutive_retransmissions++;
            RTO *= 2;
        }
        timer.reset(RTO);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { 
    return _consecutive_retransmissions;
 }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
