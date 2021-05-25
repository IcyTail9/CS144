#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//ctor
//set the buffer size as _capacity with \param[in] capacity
//in fact you can set buffer size very large 
//maximum limit is 1024 bytes
ByteStream::ByteStream(const size_t capacity) : _capacity(capacity){}

size_t ByteStream::write(const string &data) {
    size_t write_count = data.size();
    if (write_count > remaining_capacity())
        write_count = remaining_capacity();
    _buffers.append(BufferList(move(string().assign(data.begin(), data.begin() + write_count)))); 
    _bytes_written += write_count;
    return write_count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    const size_t peek_length = len > buffer_size() ? buffer_size() : len;
    string str = _buffers.concatenate();
    return string().assign(str.begin(), str.begin() + peek_length);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_length = len > buffer_size() ? buffer_size() : len;
    _buffers.remove_prefix(pop_length);
    _bytes_read += pop_length;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t read_lenth = len > buffer_size() ? buffer_size() : len;
    string read_str = peek_output(read_lenth);
    pop_output(read_lenth);
    return read_str;
}

void ByteStream::end_input() { _inputEnded = true;}

bool ByteStream::input_ended() const { return _inputEnded; }

size_t ByteStream::buffer_size() const { return _buffers.size(); }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _inputEnded && (buffer_size()==0);}

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity- buffer_size(); }



