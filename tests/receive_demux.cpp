#include "ReceiveDemux.h"
#include <array>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <vector>

using Bytes=std::vector<unsigned char>;
namespace {
    void Check(bool condition,const char *message) { if(!condition) throw std::runtime_error(message); }
    Bytes Private(unsigned char value) {
        Bytes result(std::begin(DYNAMITE_RAW_HEADER),std::end(DYNAMITE_RAW_HEADER));
        result.push_back(value); return result;
    }
}
int main() {
    try {
        std::array<unsigned char,128> buffer{};
        std::deque<Bytes> packets;
        unsigned consumed=0,reads=0;
        auto receive=[&] {
            return Dynamite::ReceiveGameDatagram(buffer.data(),buffer.size(),[&]() -> int32_t {
                ++reads;
                if(packets.empty()) return -1;
                auto data=packets.front(); packets.pop_front();
                std::memcpy(buffer.data(),data.data(),data.size()); return data.size();
            },[&](void *data,int32_t size) {
                Check(size>=8 && std::memcmp(data,DYNAMITE_RAW_HEADER,8)==0,"native packet consumed by mod");
                ++consumed;
            });
        };
        const Bytes movement{1,2,3,4,5,6,7,8,9,10};
        packets={Private(1),Private(2),movement};
        Check(receive()==10 && consumed==2,"private packet leaked to native decoder");
        Check(std::memcmp(buffer.data(),movement.data(),movement.size())==0,"native movement payload altered");
        Check(reads==3,"movement delayed behind private packets");
        packets={Private(3),Bytes{9}};
        Check(receive()==1 && buffer[0]==9,"short native packet was lost");
        // Even a malformed private payload must not be offered to MGSV's parser.
        packets={Bytes(std::begin(DYNAMITE_RAW_HEADER),std::end(DYNAMITE_RAW_HEADER)),movement};
        Check(receive()==10,"malformed private frame leaked");
        packets.clear();
        for(unsigned i=0;i<40;++i) packets.push_back(Private(i));
        packets.push_back(movement);
        const auto before=reads;
        Check(receive()==-1 && reads-before==32,"private flood did not yield the network job");
        Check(receive()==10 && packets.empty(),"movement starved after a private burst");
        Check(receive()==-1,"empty native receive result changed");
        Check(Dynamite::ReceiveGameDatagram(nullptr,0,[] {return -2;},[](void *,int32_t){})==-2,"native error changed");
        std::cout << "PASS: native/private UDP isolation, movement after private bursts, short packets, malformed private frames, bounded drain and errors\n";
    } catch(const std::exception &error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
