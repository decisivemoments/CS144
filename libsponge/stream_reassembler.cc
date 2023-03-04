#include "stream_reassembler.hh"
#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), 
    _capacity(capacity), _wait_index(0), _eof_index(-1) {
}


void StreamReassembler::link_str(const std::string &data){
    if(_wait_index + data.size() - _output.bytes_read() > _capacity){
        std:: string insert_str = data.substr(0, _output.bytes_read() + _capacity - _wait_index);
        std::size_t len = insert_str.size();
        try {
            std::size_t write_len = _output.write(insert_str);
            if(write_len != len){
                printf("link str size error, want to weite %s, but write %lu bytes\n", insert_str.c_str(), write_len);
            }
            _wait_index += write_len;
        } catch (const char *msg) {
            std::cout << msg << std::endl;
            exit(-1);
        }
        return;
    }
    try {
        std::size_t write_len = _output.write(data);
        if(write_len != data.size()){
            printf("link str size error, want to weite %s, but write %lu bytes\n", data.c_str(), write_len);
        }
        _wait_index += data.size();
    } catch (const char *msg) {
        std::cout << msg << std::endl;
        exit(-1);
    }
}

void StreamReassembler::to_wait(const std::size_t index, std::string data){
    //check if the str length is greater than the capability
    if(index + data.size() > _output.bytes_read() + _capacity){
        data = data.substr(0, _output.bytes_read() + _capacity - index);
    }
    std::map<std::size_t, std::string>::iterator itor = _wait_str.find(index);
    if(itor == _wait_str.end()){
        _wait_str[index] = data;
    }
    else if(itor->first == index){
        if(data.size() <= itor->second.size()){
            return;
        }
        itor->second = data;
    }
    for(itor = _wait_str.begin(); itor != _wait_str.end();){
        std::map<size_t, std::string>::iterator itor_prev = itor++;
        if(itor == _wait_str.end()){
            break;
        }
        if(itor_prev->second.size() + itor_prev->first < itor->first){
            continue;
        }else if(itor_prev->second.size() + itor_prev->first == itor->first){
            std::string linked_str = itor_prev->second + itor->second;
            itor_prev->second = linked_str;
            itor = _wait_str.erase(itor);
            itor = itor_prev;
        }else{
            if(itor_prev->first + itor_prev->second.size() >= itor->first + itor->second.size()){
                itor = _wait_str.erase(itor);
                itor = itor_prev;
            }else{
                std::size_t position = itor->first + itor->second.size() - itor_prev->first - itor_prev->second.size();
                position = itor->second.size() - position;
                std::string linked_str = itor_prev->second + itor->second.substr(position);
                itor_prev->second = linked_str;
                itor = _wait_str.erase(itor);
                itor = itor_prev;
            }
        }
    }
}

void StreamReassembler::check_wait_to_link(){
    for(std::map<std::size_t, std::string>::iterator itor = _wait_str.begin(); itor != _wait_str.end();){
        if(itor->first > _wait_index){
            break;
        }else if(itor->first == _wait_index){
            link_str(itor->second);
            itor = _wait_str.erase(itor);
        }else{
            if(itor->second.size() + itor->first > _wait_index){
                std::string insert_str = itor->second.substr(_wait_index - itor->first);
                link_str(insert_str);
            }
            itor = _wait_str.erase(itor);
        }
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if(eof){
        _eof_index = index + data.size();
    }
    if(data.size()){
        if(index > _wait_index){
            to_wait(index, data);
            return;
        }else if(index < _wait_index){
            if(data.size() + index <= _wait_index){
                return;
            }
            std::string insert_str = data.substr(_wait_index - index);
            link_str(insert_str);    

        }else{
            link_str(data);
        }    
    }
    check_wait_to_link();
    if(empty() && _wait_index == _eof_index){
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { 
    std::size_t len = 0;
    for(auto itor = _wait_str.begin();itor != _wait_str.end(); ++itor){
        len += itor->second.size();
    }
    return len;
 }

bool StreamReassembler::empty() const { 
    return _wait_str.empty();
 }
