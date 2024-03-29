#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

class Timer {
  private:
    unsigned int _timeout;
    unsigned int _elapsed;
    bool _expired;
    bool _started;

  public:
    Timer(unsigned int timeout) : _timeout(timeout), _elapsed(0), _expired(false), _started(false) {}

    void tick(const size_t ms_since_last_tick) {
        _elapsed += ms_since_last_tick;
        // printf("elapsed:%u, timeout:%u, started:%d\n", _elapsed, _timeout, _started);
        if (_started && _elapsed >= _timeout) {
            _expired = true;
        }
    }

    void reset(unsigned int timeout) {
        _elapsed = 0;
        _expired = false;
        _started = true;
        _timeout = timeout;
    }

    void close() { _started = false; }

    bool expired() const { return _expired; }

    bool started() const { return _started; }
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    unsigned int RTO;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    //! the segments that has been sent but not yet acknowledged
    std::queue<TCPSegment> _segments_in_flight{};

    //! recent window size
    uint16_t _window_size{1};

    //! expected ackno
    uint64_t _ackno{0};

    //! the number of consecutive retransmissions
    unsigned int _consecutive_retransmissions{0};

    //! the number of bytes in flight
    size_t _bytes_in_flight{0};

    //! timer for retransmission
    Timer timer;

    //! whether FIN has sent
    bool _fin_sent{false};

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}

    //! \brief whether FIN has sent
    bool fin_sent() const { return _fin_sent; }

    //! \brief whether FIN has been acknowledged
    bool fin_acked() const { return _fin_sent && (_ackno == _next_seqno); }

    //! \brief whether SYN has sent
    bool syn_sent() const { return _next_seqno > 0; }
};


#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
