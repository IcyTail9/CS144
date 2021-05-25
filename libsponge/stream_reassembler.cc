#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

// TODO: reslove Test #26: t_byte_stream_capacity ...........   Passed    0.72 sec  OVERTIME!!!!
// TODO: find a beautiful method to emplace many temporary variables in push_substring()
using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) { 
    size_t fixed_len = data.length();
    size_t start_index = index;
    size_t end_index = index + fixed_len;
    size_t _first_unaccept = _output.bytes_read()+_capacity;
    // ignore input
    if (start_index >= _first_unaccept)
        return;

    size_t pre_offset = start_index < _first_unassem ? _first_unassem - start_index : 0;
    size_t end_offset = end_index > _first_unaccept ? end_index - _first_unaccept : 0;
    
    start_index += pre_offset;
    end_index -= end_offset;
    if( fixed_len < pre_offset + end_offset)
        return;
    
 
    fixed_len -= (pre_offset + end_offset);
    string fixed_data = data.substr(pre_offset,fixed_len);
  
    auto i = _ressembler.begin();
    //auto i = _ressembler.upper_bound(move(substrings("",start_index)));
    //if(i != _ressembler.begin() && i != _ressembler.end())
    //    i--;
    while(i != _ressembler.end()) {
        
        if(i->_start_index > end_index)
            break;
        // has no overlap, i is in front of new_str
        if (i->_end_index < start_index)
            ++i;//TODO: try binary search to exceed
        // i-> _end_index >= start_index
        else if (i->_start_index <= start_index) {
            // new_str is a part of i, no need to write or reassem
            if (i->_end_index >= end_index)
                goto eof_check;
            fixed_data = i->_data.substr(0, start_index - i->_start_index) + fixed_data;
            _bytes_unassem -= i->_data.length();
            start_index = i->_start_index;
            i = _ressembler.erase(i);
        } 
        else if (i->_start_index > start_index) {
            if (i->_end_index >= end_index) {
                fixed_data = fixed_data + i->_data.substr(end_index - i->_start_index);
                end_index = i->_end_index;
                
            }
            _bytes_unassem -= i->_data.length();
            i = _ressembler.erase(i);
        }
    }
    // decide to write or push in reasembler
    if(start_index == _first_unassem){
        _first_unassem += _output.write(fixed_data);
    }
    else{
        _ressembler.emplace(i,substrings{fixed_data, start_index});
        _bytes_unassem += fixed_data.length();
    }
    eof_check:
    if(eof)
        _eof = true;
    if(end_offset > 0)
        _eof = false;
    if (_eof && empty())
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _bytes_unassem; }

bool StreamReassembler::empty() const { return _bytes_unassem == 0 ; }