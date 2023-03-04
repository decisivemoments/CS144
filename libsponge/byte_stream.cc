#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : buffer(), capability(capacity), input_is_end(false), _bytes_written(0), _bytes_read(0), _error(false) {}

size_t ByteStream::write(const string &data) {
    if (buffer.length() + data.length() <= capability) {
        buffer.append(data);
        _bytes_written += data.length();
        // printf("write in %s\n", data.c_str());
        return data.length();
    } else {
        unsigned this_write = capability - buffer.length();
        buffer.append(data.substr(0, this_write));
        _bytes_written += this_write;
        // printf("write in %s\n", data.substr(0, this_write).c_str());
        return this_write;
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (len <= buffer.length()) {
        return buffer.substr(0, len);
    } else {
        return buffer;
    }
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len <= buffer.length()) {
        buffer = buffer.substr(len);
        _bytes_read += len;
    } else {
        _bytes_read += buffer.length();
        buffer.clear();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string return_string = peek_output(len);
    pop_output(len);
    return return_string;
}

void ByteStream::end_input() { input_is_end = true; }

bool ByteStream::input_ended() const { return input_is_end; }

size_t ByteStream::buffer_size() const { return buffer.length(); }

bool ByteStream::buffer_empty() const { return buffer.length() == 0; }

bool ByteStream::eof() const {
    if (buffer.length() == 0 && input_is_end) {
        return true;
    } else {
        return false;
    }
}

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return capability - buffer.length(); }
