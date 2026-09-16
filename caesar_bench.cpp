#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>
#include <caesar/consensus.hpp>
using namespace caesar;
int main(){std::vector<std::uint8_t> h{0x43,0x5a,0x52,0x01,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80};std::uint64_t n=0;Hash256 last{};auto s=std::chrono::steady_clock::now();auto e=s+std::chrono::seconds(5);while(std::chrono::steady_clock::now()<e){last=calculate_pow_hash(h,n++);}double t=std::chrono::duration<double>(std::chrono::steady_clock::now()-s).count();double r=n/t;std::cout<<"=== Caesar CZR PoW Benchmark ===\n"<<"Elapsed: "<<std::fixed<<std::setprecision(3)<<t<<" s\n"<<"Attempts: "<<n<<"\n"<<"Hashrate: "<<std::fixed<<std::setprecision(2)<<r<<" H/s\n"<<"Hashrate: "<<std::fixed<<std::setprecision(4)<<(r/1000.0)<<" kH/s\n"<<"Final hash: "<<hash_to_hex(last)<<"\n";}
