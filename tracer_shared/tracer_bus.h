#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <string>
namespace pawtracer {
class Bus {
 public:
  enum Field { PV_V, PV_A, PV_W, BAT_V, BAT_A, BAT_T, CTRL_T, SOC, CURRENT_LOW, COUNT };
  std::array<float,COUNT> values{};
  std::array<uint32_t,COUNT> stamps{}, revisions{};
  uint32_t matched=0, orphan=0, exceptions=0;
  bool polling=false;
  std::string dashboard(uint32_t now) const {
    std::string out="{\"mode\":\"";
    out += polling ? "Poll - no dongle" : "Listen - Solar Guardian";
    out += "\",\"values\":[";
    for(size_t i=0;i<COUNT;i++){if(i)out+=",";out+=std::isfinite(values[i])?std::to_string(values[i]):"null";}
    out += "],\"ages\":[";
    for(size_t i=0;i<COUNT;i++){if(i)out+=",";auto a=age(i,now);out+=std::isfinite(a)?std::to_string(a):"null";}
    out += "],\"matched\":"+std::to_string(matched)+",\"orphan\":"+std::to_string(orphan)+",\"exceptions\":"+std::to_string(exceptions)+"}";
    return out;
  }

  Bus(){values.fill(NAN);}
  static uint16_t crc(const uint8_t *p,size_t n){uint16_t c=0xffff;while(n--){c^=*p++;for(int i=0;i<8;i++)c=(c&1)?(c>>1)^0xa001:c>>1;}return c;}
  static bool valid(const uint8_t *p,size_t n){return n>=4 && crc(p,n-2)==uint16_t(p[n-2]|(p[n-1]<<8));}
  void mode(bool active){polling=active;pending_=false;buf_.clear();next_=0;last_tx_=0;}
  float age(size_t f,uint32_t now)const{return revisions[f]?float(uint32_t(now-stamps[f]))/1000.f:NAN;}
  void feed(uint8_t b,uint32_t now){
    if(!buf_.empty() && uint32_t(now-last_byte_)>200){buf_.clear();pending_=false;}
    last_byte_=now;
    if(buf_.size()>=260){buf_.clear();pending_=false;}
    buf_.push_back(b);parse(now);
  }
  // Caller writes returned request through UART hardware direction control.
  std::vector<uint8_t> poll(uint32_t now){
    if(!polling)return {};
    if(pending_ && uint32_t(now-request_at_)<=1500)return {};
    if(uint32_t(now-last_tx_)<1000)return {};
    static const uint16_t start[]={0x3100,0x3104,0x3110,0x311a,0x331a,0x331b};
    static const uint16_t count[]={4,2,2,1,1,2};
    uint16_t a=start[next_],n=count[next_];next_=(next_+1)%6;
    std::vector<uint8_t> r={1,4,uint8_t(a>>8),uint8_t(a),0,uint8_t(n)};
    auto c=crc(r.data(),r.size());r.push_back(c&255);r.push_back(c>>8);
    expect(4,a,n,now);last_tx_=now;return r;
  }
 private:
  std::vector<uint8_t> buf_;
  uint32_t last_byte_=0,request_at_=0,last_tx_=0;
  uint16_t start_=0,count_=0;
  uint8_t fn_=0,next_=0;
  bool pending_=false;
  void expect(uint8_t fn,uint16_t a,uint16_t n,uint32_t now){fn_=fn;start_=a;count_=n;request_at_=now;pending_=true;}
  void set(Field f,float v,uint32_t now){values[f]=v;stamps[f]=now;++revisions[f];}
  void decode(const uint8_t *p,uint32_t now){
    auto has=[&](uint16_t a,unsigned n=1){return a>=start_ && uint32_t(a)+n<=uint32_t(start_)+count_;};
    auto word=[&](uint16_t a)->uint16_t {size_t i=3+2*(a-start_);return uint16_t(p[i]<<8|p[i+1]);};
    if(fn_!=4)return;
    if(has(0x3100))set(PV_V,word(0x3100)*.01f,now);
    if(has(0x3101))set(PV_A,word(0x3101)*.01f,now);
    if(has(0x3102,2))set(PV_W,(uint32_t(word(0x3103))<<16|word(0x3102))*.01f,now);
    if(has(0x3104))set(BAT_V,word(0x3104)*.01f,now);
    if(has(0x3105))set(BAT_A,word(0x3105)*.01f,now);
    if(has(0x3110))set(BAT_T,int16_t(word(0x3110))*.01f,now);
    if(has(0x3111))set(CTRL_T,int16_t(word(0x3111))*.01f,now);
    if(has(0x311a))set(SOC,word(0x311a),now);
    if(has(0x331a))set(BAT_V,word(0x331a)*.01f,now);
    if(has(0x331b))set(CURRENT_LOW,word(0x331b)*.01f,now);
    // Full signed 32-bit register only; never infer sign from a low-word-only reply.
    if(has(0x331b,2))set(BAT_A,int32_t(uint32_t(word(0x331c))<<16|word(0x331b))*.01f,now);
  }
  void parse(uint32_t now){
    while(buf_.size()>=2){
      if(buf_[0]!=1){buf_.erase(buf_.begin());continue;}
      uint8_t f=buf_[1];
      if(f!=2 && f!=4 && f!=0x82 && f!=0x84){pending_=false;buf_.erase(buf_.begin());continue;}
      if(buf_.size()<3)return;
      size_t n=0;
      // Requests and replies share address/function. CRC distinguishes candidates.
      if((f==2||f==4) && buf_.size()>=8 && valid(buf_.data(),8)){
        uint16_t a=uint16_t(buf_[2]<<8|buf_[3]),q=uint16_t(buf_[4]<<8|buf_[5]);
        if(q && q<=(f==4?125:2000)){expect(f,a,q,now);n=8;}
      }
      if(!n){
        size_t len=(f&0x80)?5:size_t(buf_[2])+5;
        if(len>255 || (!(f&0x80) && (buf_[2]==0 || (f==4 && (buf_[2]&1))))){
          if(buf_.size()<8)return;
          pending_=false;buf_.erase(buf_.begin());continue;
        }
        if(buf_.size()<len)return;
        if(!valid(buf_.data(),len)){
          if(buf_.size()<8)return;
          pending_=false;buf_.erase(buf_.begin());continue;
        }
        n=len;
        bool fresh=pending_ && uint32_t(now-request_at_)<=1500 && (f&0x7f)==fn_;
        if(f&0x80){if(fresh){++exceptions;pending_=false;}}
        else if(fresh && buf_[2]==(fn_==4?2*count_:(count_+7)/8)){
          decode(buf_.data(),now);++matched;pending_=false;
        }else {++orphan;pending_=false;}
      }
      buf_.erase(buf_.begin(),buf_.begin()+n);
    }
  }
};
inline Bus &bus(){static Bus b;return b;}
}
