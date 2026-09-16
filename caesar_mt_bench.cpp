#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>
#include <caesar/consensus.hpp>
using namespace caesar;
int main(){std::vector<std::uint8_t> h{0x43,0x5a,0x52,0x01,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80};unsigned int tc=std::thread::hardware_concurrency();if(!tc)tc=2;std::atomic<bool> stop{false};std::atomic<std::uint64_t> total{0};std::vector<std::thread> workers;auto s=std::chrono::steady_clock::now();auto e=s+std::chrono::seconds(5);for(unsigned int t=0;t<tc;++t)workers.emplace_back([&,t](){std::uint64_t n=static_cast<std::uint64_t>(t)<<48;while(!stop.load(std::memory_order_relaxed)){calculate_pow_hash(h,n++);total.fetch_add(1,std::memory_order_relaxed);}});while(std::chrono::steady_clock::now()<e)std::this_thread::yield();stop.store(true);for(auto& w:workers)w.join();double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-s).count();double r=static_cast<double>(total.load())/sec;std::cout<<"=== Caesar CZR Multi-Thread Benchmark ===\n"<<"Threads: "<<tc<<"\n"<<"Elapsed: "<<std::fixed<<std::setprecision(3)<<sec<<" s\n"<<"Attempts: "<<total.load()<<"\n"<<"Hashrate: "<<std::fixed<<std::setprecision(2)<<r<<" H/s\n"<<"Hashrate: "<<std::fixed<<std::setprecision(4)<<(r/1000.0)<<" kH/s\n";}
