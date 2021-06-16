#include "stream_reassembler.hh"

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) { 
    
    size_t _first_unaccept = _output.bytes_read()+_capacity;
    bool eof_if_no_cut = eof;
    substrings new_str{data,index};
    
    auto i = _reassembler.begin();

    // ignore input
    if (index >= _first_unaccept or data.length() == 0)
        goto eof_check;
    // refact new_str (cut off the end)
    if (new_str._end_index > _first_unaccept) {
        new_str._data = new_str._data.substr(0,_first_unaccept - index);
        new_str._end_index = _first_unaccept;
        eof_if_no_cut = false;
    }
    // refact new_str (cut off the head)
    if (new_str._start_index < _first_unassem ) {
        if(new_str._end_index <= _first_unassem)
            return;
        new_str._data = new_str._data.substr(_first_unassem - new_str._start_index);
        new_str._start_index = _first_unassem;
        new_str._end_index = _first_unassem + new_str._data.length();
    }
    
    // i-> end <= new_str.start , i is the last sub_str whose end_index <= new_str.start_index
    
    // i->end_index > new_str.start_index
    // i = _reassembler.lower_bound(new_str);
    while(i != _reassembler.end()){
        if(new_str._end_index <= i->_start_index)
            break;
        else if(new_str._start_index >= i->_end_index)            
            ++i;
        // new_str._start_index < i->_end_index
        // i has a stream part that new_str has not at begin
        else if(new_str._start_index > i ->_start_index){
            // new_str is a part of i
            if(new_str._end_index < i->_end_index)
                goto eof_check;
            new_str._data = new_str._data.substr(i -> _end_index- new_str._start_index);
            new_str._start_index = i->_end_index;
            ++i;
        }
        // i has a stream hat new_str has not in the end
        else if( new_str._end_index < i->_end_index){
            new_str._data = new_str._data.substr(0,i->_start_index - new_str._start_index);
            new_str._end_index = i->_start_index;
            break;
        }
        // i is a part of new_str
        else{
            _bytes_unassem -= i->_data.length();
            i = _reassembler.erase(i);
        }
    }
    _bytes_unassem += new_str._data.length();
    _reassembler.emplace(i,move(new_str));
    //_reassembler.emplace_hint(i,move(new_str));
    
    // decide to write or push in reasembler
    while(not _reassembler.empty() and _reassembler.begin()->_start_index == _first_unassem){
        size_t write_len = _output.write(_reassembler.begin()->_data);
        _first_unassem += write_len;
        _bytes_unassem -= write_len;
        _reassembler.pop_front();
        //_reassembler.erase(_reassembler.begin());
    }
   
    eof_check:
    _eof = _eof || eof_if_no_cut;

    if (_eof && empty())
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _bytes_unassem; }

bool StreamReassembler::empty() const { return _bytes_unassem == 0 ; }

