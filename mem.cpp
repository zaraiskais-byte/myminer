#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <caesar/consensus.hpp>
std::uint64_t rss(){std::ifstream f("/proc/self/status");std::string s;while(std::getline(f,s))if(s.rfind("VmRSS:",0)==0){std::uint64_t v=0;sscanf(s.c_str(),"VmRSS: %llu",&v);return v;}return 0;}
int main(){std::cout<<"RSS before: "<<rss()<<" KB\n";std::thread t([](){std::vector<std::uint8_t> h{0x43,0x5a,0x52,1,0x10,0x20,0x30,0x40};auto e=std::chrono::steady_clock::now()+std::chrono::seconds(5);std::uint64_t n=0;while(std::chrono::steady_clock::now()<e)caesar::calculate_pow_hash(h,n++);});std::this_thread::sleep_for(std::chrono::seconds(2));std::cout<<"RSS during PoW: "<<rss()<<" KB\n";t.join();std::cout<<"RSS after: "<<rss()<<" KB\n";std::cout<<"PoW memory target: "<<(caesar::CZR_POW_MEMORY_WORDS*8)<<" bytes\n";}
