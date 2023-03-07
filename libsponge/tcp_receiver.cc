#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader& header = seg.header();
    const ByteStream& innerBS = _reassembler.stream_out();
    const Buffer& payload = seg.payload();
    bool isend = header.fin;
    uint64_t abseq;
    if(isn == -1 && header.syn == false){
        return;
    }
    if(header.syn){
        abseq = 1;
        isn = static_cast<int64_t>(header.seqno.raw_value());
    }else{
        abseq = unwrap(header.seqno, 
        WrappingInt32(static_cast<uint32_t>(isn)), 
        innerBS.bytes_written());
    }
    _reassembler.push_substring(std::string{payload.str()}, abseq - 1, isend);
    if(isend){
        fsn = abseq + seg.length_in_sequence_space() - 1;
        if(header.syn)
            fsn -= 1;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(isn == -1){
        return nullopt;
    }
    if(isn < 0 || isn > UINT32_MAX){
        printf("isn num error: %ld\n", isn);
    }
    const ByteStream& innerBS = _reassembler.stream_out();
    if(fsn != -1 && innerBS.bytes_written() + 1 == static_cast<uint64_t>(fsn)){
        return wrap(innerBS.bytes_written() + 2, WrappingInt32(static_cast<uint32_t>(isn)));
    }
    return wrap(innerBS.bytes_written() + 1, WrappingInt32(static_cast<uint32_t>(isn)));
}

size_t TCPReceiver::window_size() const { 
    const ByteStream& innerBS = _reassembler.stream_out();
    size_t res = _capacity - (innerBS.bytes_written() - innerBS.bytes_read());
    return res;
 }
