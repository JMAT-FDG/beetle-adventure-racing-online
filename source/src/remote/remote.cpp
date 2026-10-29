// Host-rendered remote play. One simulation, bounded latest-frame queues, four pad slots.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include "remote.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_MAX_DIMENSIONS 4096
#include "stb_image.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace bar::remote {
namespace {
using Bytes = std::vector<uint8_t>;
enum Type : uint8_t { Challenge=1, Hello, Welcome, Input, Frame, Control, Audio };
constexpr uint32_t max_message = 8 * 1024 * 1024;
uint64_t now() { return GetTickCount64(); }
void network_init() {
    static const bool ready = [] { WSADATA w{}; return WSAStartup(MAKEWORD(2,2), &w)==0; }();
    if (!ready) throw std::runtime_error("Winsock initialization failed");
}
void put16(Bytes& b, uint16_t n) { b.push_back(uint8_t(n>>8)); b.push_back(uint8_t(n)); }
uint16_t get16(const uint8_t* p) { return uint16_t((p[0]<<8)|p[1]); }
void put32(Bytes& b, uint32_t n) { for (int i=3;i>=0;--i) b.push_back(uint8_t(n>>(8*i))); }
uint32_t get32(const uint8_t* p) { return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3]; }
bool transfer(SOCKET s, uint8_t* data, size_t size, bool sending) {
    while (size) {
        int n = sending ? ::send(s, reinterpret_cast<const char*>(data), int(std::min<size_t>(size,65536)),0)
                        : ::recv(s, reinterpret_cast<char*>(data), int(std::min<size_t>(size,65536)),0);
        if (n<=0) return false;
        data+=n; size-=size_t(n);
    }
    return true;
}
bool send_message(SOCKET s, Type type, const Bytes& b) {
    if (b.size()>max_message) return false;
    Bytes header{'B','A','R','1',1,uint8_t(type),0,0}; put32(header,uint32_t(b.size()));
    return transfer(s,header.data(),header.size(),true) &&
           (b.empty() || transfer(s,const_cast<uint8_t*>(b.data()),b.size(),true));
}
bool receive_message(SOCKET s, Type& type, Bytes& b) {
    uint8_t h[12];
    if (!transfer(s,h,sizeof(h),false) || std::memcmp(h,"BAR1",4) || h[4]!=1 || h[6] || h[7]) return false;
    auto size=get32(h+8);
    if (size>max_message || h[5]<Challenge || h[5]>Audio) return false;
    type=Type(h[5]); b.resize(size);
    return !size || transfer(s,b.data(),b.size(),false);
}
void socket_options(SOCKET s) {
    DWORD ms=3000; BOOL yes=TRUE;
    setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&ms),sizeof(ms));
    setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&ms),sizeof(ms));
    setsockopt(s,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&yes),sizeof(yes));
}
Bytes random_bytes(size_t n) {
    Bytes b(n);
    if (BCryptGenRandom(nullptr,b.data(),ULONG(n),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)
        throw std::runtime_error("System random generator failed");
    return b;
}
bool valid_token(const std::string& token) {
    return token.size()==32 && std::all_of(token.begin(),token.end(),[](char c) {
        return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');
    });
}
Bytes mac(const std::string& key, const Bytes& data) {
    BCRYPT_ALG_HANDLE alg=nullptr; BCRYPT_HASH_HANDLE hash=nullptr; Bytes out(32);
    NTSTATUS status=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (status>=0) status=BCryptCreateHash(alg,&hash,nullptr,0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),ULONG(key.size()),0);
    if (status>=0) status=BCryptHashData(hash,const_cast<PUCHAR>(data.data()),ULONG(data.size()),0);
    if (status>=0) status=BCryptFinishHash(hash,out.data(),ULONG(out.size()),0);
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg,0);
    if (status<0) throw std::runtime_error("HMAC-SHA256 failed");
    return out;
}
bool equal_mac(const uint8_t* a,const uint8_t* b) { uint8_t d=0; for (int i=0;i<32;++i) d|=a[i]^b[i]; return d==0; }
int env_int(const char* name,int fallback,int low,int high) {
    const char* p=std::getenv(name); if (!p) return fallback;
    char* end=nullptr; long n=std::strtol(p,&end,10);
    if (!end || *end || n<low || n>high) throw std::runtime_error(std::string("Invalid ")+name);
    return int(n);
}
struct Peer {
    SOCKET socket=INVALID_SOCKET;
    int first=0,count=0;
    bool mirror=false;
    std::array<int,4> views{};
    std::atomic<bool> alive{true};
    std::mutex mutex; std::condition_variable cv;
    std::array<Pad,4> pads{}; uint64_t input_time=0;
    std::vector<Bytes> frames; Bytes pcm; int audio_rate=0;
    std::thread reader,writer;
    void disconnect() { if (alive.exchange(false)) shutdown(socket,SD_BOTH); cv.notify_all(); }
    void join() { if (reader.joinable()) reader.join(); if (writer.joinable()) writer.join(); }
    ~Peer() { disconnect(); join(); if(socket!=INVALID_SOCKET) closesocket(socket); }
};
struct RawFrame { uint32_t width=0,height=0; Bytes bgra; };
struct Host {
    SOCKET listener=INVALID_SOCKET; std::atomic<bool> running{true};
    int locals=1, width=1920,height=1080,fps=30,quality=85;
    std::string token;
    std::atomic<bool> active{false}; std::atomic<int> layout{1}; std::atomic<uint64_t> capture_time{0};
    std::mutex peers_mutex,frame_mutex; std::condition_variable frame_cv;
    std::vector<std::shared_ptr<Peer>> peers; RawFrame pending; bool have_frame=false;
    std::thread accept_thread,encode_thread;
    std::vector<std::shared_ptr<Peer>> clients() { std::lock_guard lock(peers_mutex); return peers; }
    bool any_client() { for (auto& p:clients()) if(p->alive) return true; return false; }
    void command(int cmd) { if(cmd==0) active=!active.load(); else if(cmd==1) layout=layout.load()%3+1;
        std::fprintf(stderr,"[remote] views=%s layout=%d\n",active?"on":"off",layout.load()); }
    void reader(const std::shared_ptr<Peer>& p) {
        Type type{}; Bytes data;
        while (running && p->alive && receive_message(p->socket,type,data)) {
            if (type==Input && data.size()==size_t(p->count)*4) {
                std::lock_guard lock(p->mutex);
                for(int i=0;i<p->count;++i) p->pads[i]={get16(data.data()+i*4),int8_t(data[i*4+2]),int8_t(data[i*4+3])};
                p->input_time=now();
            } else if(type==Control && data.size()==1 && data[0]<=1 && (p->mirror || p->first==0)) {
                command(data[0]);
            } else break;
        }
        p->disconnect();
    }
    void writer(const std::shared_ptr<Peer>& p) {
        while(running && p->alive) {
            std::vector<Bytes> frames; Bytes audio; int rate=0;
            {
                std::unique_lock lock(p->mutex);
                p->cv.wait(lock,[&]{return !p->alive || !p->frames.empty();});
                if(!p->alive) break;
                frames.swap(p->frames); audio.swap(p->pcm); rate=p->audio_rate;
            }
            bool ok=true;
            for(auto& f:frames) if(!(ok=send_message(p->socket,Frame,f))) break;
            if(ok && !audio.empty()) { Bytes a; put32(a,uint32_t(rate)); a.insert(a.end(),audio.begin(),audio.end()); ok=send_message(p->socket,Audio,a); }
            if(!ok) break;
        }
        p->disconnect();
    }
    void accept_loop() {
        while(running) {
            sockaddr_in address{}; int len=sizeof(address);
            SOCKET s=accept(listener,reinterpret_cast<sockaddr*>(&address),&len);
            if(s==INVALID_SOCKET) break;
            socket_options(s);
            // Reap completed connections outside the mutex used by the game input thread.
            std::vector<std::shared_ptr<Peer>> dead;
            { std::lock_guard lock(peers_mutex); auto it=peers.begin(); while(it!=peers.end()) {
                if(!(*it)->alive) {dead.push_back(*it); it=peers.erase(it);} else ++it;
            }}
            for(auto& p:dead) p->join();
            try {
                Bytes nonce=random_bytes(32),hello; Type type{};
                if(!send_message(s,Challenge,nonce) || !receive_message(s,type,hello) || type!=Hello || hello.size()!=38)
                    throw std::runtime_error("Handshake rejected");
                Bytes authenticated=nonce; authenticated.insert(authenticated.end(),hello.begin(),hello.begin()+6);
                auto expected=mac(token,authenticated);
                if(!equal_mac(expected.data(),hello.data()+6)) throw std::runtime_error("Authentication rejected");
                int count=hello[1]; bool mirror=hello[0]==1;
                if(hello[0]>1 || count<1 || count>4 || (mirror && ((ntohl(address.sin_addr.s_addr)>>24)!=127)))
                    throw std::runtime_error("Invalid player request");
                int first=-1; auto existing=clients();
                if(mirror) {
                    if(count>locals) throw std::runtime_error("Too many local views");
                    for(auto& p:existing) if(p->alive && p->mirror) throw std::runtime_error("Local viewer already connected");
                    first=0;
                } else {
                    for(int start=locals;start+count<=4;++start) {
                        bool free=true;
                        for(auto& p:existing) if(p->alive&&!p->mirror&&start<p->first+p->count&&start+count>p->first) free=false;
                        if(free) {first=start;break;}
                    }
                }
                if(first<0) throw std::runtime_error("No free N64 player slots");
                auto p=std::make_shared<Peer>(); p->socket=s; s=INVALID_SOCKET; p->first=first; p->count=count; p->mirror=mirror;
                for(int i=0;i<count;++i) { p->views[i]=mirror?hello[2+i]:first+i; if(p->views[i]>3) throw std::runtime_error("Invalid view"); }
                if(!send_message(p->socket,Welcome,Bytes{uint8_t(first),uint8_t(count)})) throw std::runtime_error("Welcome failed");
                {std::lock_guard lock(peers_mutex); peers.push_back(p);}
                p->reader=std::thread([this,p]{reader(p);}); p->writer=std::thread([this,p]{writer(p);});
                std::fprintf(stderr,"[remote] %s connected: players %d..%d\n",mirror?"local viewer":"peer",first+1,first+count);
            } catch(const std::exception& e) { std::fprintf(stderr,"[remote] %s\n",e.what()); if(s!=INVALID_SOCKET) closesocket(s); }
        }
    }
    Bytes encode_view(const RawFrame& f,int mode,int player,int local_index) {
        Rect r=view_rect(mode,player,int(f.width),int(f.height));
        const double scale=std::min({1.0,double(width)/r.width,double(height)/r.height});
        int w=std::max(1,int(r.width*scale)),h=std::max(1,int(r.height*scale));
        Bytes rgb(size_t(w)*h*3);
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            size_t src=(size_t(r.y+y*r.height/h)*f.width+r.x+x*r.width/w)*4, dst=(size_t(y)*w+x)*3;
            rgb[dst]=f.bgra[src+2]; rgb[dst+1]=f.bgra[src+1]; rgb[dst+2]=f.bgra[src];
        }
        Bytes result{uint8_t(local_index)}; put16(result,uint16_t(w)); put16(result,uint16_t(h));
        auto callback=[](void* ctx,void* data,int n) { auto& bytes=*static_cast<Bytes*>(ctx); auto* p=static_cast<uint8_t*>(data); bytes.insert(bytes.end(),p,p+n); };
        if(!stbi_write_jpg_to_func(callback,&result,w,h,3,rgb.data(),quality) || result.size()>max_message) return {};
        return result;
    }
    void encode_loop() {
        while(running) {
            RawFrame f;
            { std::unique_lock lock(frame_mutex); frame_cv.wait(lock,[&]{return !running||have_frame;});
              if(!running) break; std::swap(f,pending); have_frame=false; }
            int mode=active?layout.load():0;
            std::array<Bytes,4> cache;
            for(auto& p:clients()) if(p->alive) {
                std::vector<Bytes> frames;
                for(int i=0;i<p->count;++i) {
                    int view=p->views[i];
                    if(cache[view].empty()) cache[view]=encode_view(f,mode,view,0);
                    if(!cache[view].empty()) { frames.push_back(cache[view]); frames.back()[0]=uint8_t(i); }
                }
                {std::lock_guard lock(p->mutex); p->frames=std::move(frames);} p->cv.notify_one();
            }
        }
    }
    ~Host() {
        running=false; if(listener!=INVALID_SOCKET) {closesocket(listener);listener=INVALID_SOCKET;}
        frame_cv.notify_all(); for(auto& p:clients()) p->disconnect();
        if(accept_thread.joinable()) accept_thread.join();
        if(encode_thread.joinable()) encode_thread.join();
        for(auto& p:clients()) {p->disconnect();p->join();}
    }
};
std::shared_ptr<Host> active_host;
}

Rect view_rect(int layout,int player,int w,int h) {
    if(w<1||h<1||player<0||player>3||layout<0||layout>3) throw std::runtime_error("Invalid view rectangle");
    if(layout==0) return {0,0,w,h};
    int cols=layout==1?1:2, rows=layout==2?1:2;
    int index=player%(cols*rows), x=(index%cols)*w/cols, y=(index/cols)*h/rows;
    return {x,y,(index%cols+1)*w/cols-x,(index/cols+1)*h/rows-y};
}
std::string make_token() { auto b=random_bytes(16); std::string out; const char* hex="0123456789abcdef"; for(auto v:b) {out+=hex[v>>4];out+=hex[v&15];} return out; }
bool start_host_from_environment(std::string& error) {
    if(!std::getenv("BAR_NET_HOST_PORT")) return true;
    try {
        if(std::atomic_load(&active_host)) throw std::runtime_error("Host already running");
        network_init(); auto h=std::make_shared<Host>();
        h->locals=env_int("BAR_NET_LOCAL_PLAYERS",1,0,4); h->width=env_int("BAR_VIEW_WIDTH",1920,320,2560);
        h->height=env_int("BAR_VIEW_HEIGHT",1080,240,1440); h->fps=env_int("BAR_NET_FPS",30,10,60);
        h->quality=env_int("BAR_NET_QUALITY",85,40,100); h->layout=env_int("BAR_VIEW_LAYOUT",1,1,3);
        h->active=env_int("BAR_VIEW_ACTIVE",0,0,1)!=0;
        const char* key=std::getenv("BAR_NET_TOKEN"); h->token=key?key:"";
        if(!valid_token(h->token)) throw std::runtime_error("BAR_NET_TOKEN must contain 32 hexadecimal characters");
        auto port=uint16_t(env_int("BAR_NET_HOST_PORT",28765,1024,65535));
        h->listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); if(h->listener==INVALID_SOCKET) throw std::runtime_error("Socket creation failed");
        BOOL exclusive=TRUE; setsockopt(h->listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive));
        sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=INADDR_ANY; address.sin_port=htons(port);
        if (const char* bind_address=std::getenv("BAR_NET_BIND")) {
            if (inet_pton(AF_INET,bind_address,&address.sin_addr)!=1) throw std::runtime_error("Invalid BAR_NET_BIND address");
        }
        if(bind(h->listener,reinterpret_cast<sockaddr*>(&address),sizeof(address)) || listen(h->listener,4))
            throw std::runtime_error("Host port unavailable");
        auto* instance=h.get();
        h->accept_thread=std::thread([instance]{instance->accept_loop();}); h->encode_thread=std::thread([instance]{instance->encode_loop();});
        std::atomic_store(&active_host,h);
        std::atexit(stop_host); std::fprintf(stderr,"[remote] listening on TCP %u; %d local players\n",port,h->locals);
        return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
void stop_host() { std::atomic_store(&active_host,std::shared_ptr<Host>{}); }
bool hosting() {return bool(std::atomic_load(&active_host));}
int local_players() {auto host=std::atomic_load(&active_host);return host?host->locals:4;}
bool port_connected(int port) {
    const auto host=std::atomic_load(&active_host);
    if(!host||port<0||port>3) return false;
    if(port<host->locals) return true;
    for(auto& p:host->clients()) if(p->alive && !p->mirror && port>=p->first && port<p->first+p->count) return true;
    return false;
}
bool read_pad(int port,Pad& pad) {
    const auto host=std::atomic_load(&active_host);
    if(!host) return false;
    for(auto& p:host->clients()) if(p->alive && port>=p->first && port<p->first+p->count) {
        std::lock_guard lock(p->mutex); pad=now()-p->input_time<=500?p->pads[port-p->first]:Pad{}; return true;
    }
    if(port>=host->locals) {pad={};return true;} return false;
}
bool views_active() {auto host=std::atomic_load(&active_host);return host && host->active;}
bool local_viewer_connected() {auto host=std::atomic_load(&active_host);if(host) for(auto& p:host->clients()) if(p->alive&&p->mirror)return true;return false;}
void control(int cmd) {auto host=std::atomic_load(&active_host);if(host) host->command(cmd);}
void render_size(int& w,int& h) {
    const auto host=std::atomic_load(&active_host);
    w=0;h=0;if(!host||!host->active)return;
    w=host->width*(host->layout==1?1:2); h=host->height*(host->layout==2?1:2);
}
bool frame_wanted() {
    const auto host=std::atomic_load(&active_host);
    if(!host||!host->any_client()) return false;
    uint64_t last=host->capture_time.load(), t=now();
    return t-last>=uint64_t(1000/host->fps) && host->capture_time.compare_exchange_strong(last,t);
}
void submit_bgra(const uint8_t* pixels,uint32_t w,uint32_t h,uint32_t pitch) {
    const auto host=std::atomic_load(&active_host);
    if(!host||!pixels||w<2||h<2||w>8192||h>8192||uint64_t(w)*h*4>64*1024*1024||pitch<w*4) return;
    static uint32_t previous_w=0,previous_h=0;
    if(w!=previous_w||h!=previous_h){std::fprintf(stderr,"[remote] GPU capture %ux%u\n",w,h);previous_w=w;previous_h=h;}
    RawFrame f;f.width=w;f.height=h;f.bgra.resize(size_t(w)*h*4);
    for(uint32_t y=0;y<h;++y) std::memcpy(f.bgra.data()+size_t(y)*w*4,pixels+size_t(y)*pitch,w*4);
    {std::lock_guard lock(host->frame_mutex);std::swap(host->pending,f);host->have_frame=true;}host->frame_cv.notify_one();
}
void submit_audio(const int16_t* samples,size_t count,uint32_t rate) {
    const auto host=std::atomic_load(&active_host);
    if(!host||!samples||rate<8000||rate>96000||count>65536)return;
    count &= ~size_t(1); // complete stereo frames only
    for(auto& p:host->clients()) if(p->alive) {
        std::lock_guard lock(p->mutex);if(p->audio_rate!=int(rate)||p->pcm.size()+count*2>rate) p->pcm.clear();
        p->audio_rate=int(rate);auto* bytes=reinterpret_cast<const uint8_t*>(samples);p->pcm.insert(p->pcm.end(),bytes,bytes+count*2);
    }
}

struct Client::Impl {std::atomic<SOCKET> socket{INVALID_SOCKET};int count=0,first=0;std::mutex send_mutex,audio_mutex;Bytes pcm;int rate=0;};
Client::Client():impl(std::make_unique<Impl>()){}
Client::~Client(){close();}
void Client::close(){SOCKET s=impl->socket.exchange(INVALID_SOCKET);if(s!=INVALID_SOCKET){shutdown(s,SD_BOTH);closesocket(s);}}
int Client::first_port()const{return impl->first;}
bool Client::connect(const std::string& address,uint16_t port,const std::string& token,int count,bool mirror,const std::array<int,4>& views,std::string& error){
    try {
        if(!valid_token(token)||count<1||count>4)throw std::runtime_error("Invalid session key or player count");
        network_init();addrinfo hints{},*list=nullptr;hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
        if(getaddrinfo(address.c_str(),std::to_string(port).c_str(),&hints,&list))throw std::runtime_error("Host address not found");
        SOCKET s=INVALID_SOCKET;
        for(auto* a=list;a;a=a->ai_next){s=socket(a->ai_family,a->ai_socktype,a->ai_protocol);if(s==INVALID_SOCKET)continue;
            u_long nonblock=1;ioctlsocket(s,FIONBIO,&nonblock);::connect(s,a->ai_addr,int(a->ai_addrlen));
            fd_set writable;FD_ZERO(&writable);FD_SET(s,&writable);timeval timeout{5,0};int err=0,len=sizeof(err);
            if(select(0,nullptr,&writable,nullptr,&timeout)>0 && !getsockopt(s,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&err),&len)&&!err){nonblock=0;ioctlsocket(s,FIONBIO,&nonblock);break;}
            closesocket(s);s=INVALID_SOCKET;
        }
        freeaddrinfo(list);if(s==INVALID_SOCKET)throw std::runtime_error("Could not connect to host (5 second timeout)");
        impl->socket=s;socket_options(s);Type type{};Bytes data;
        if(!receive_message(s,type,data)||type!=Challenge||data.size()!=32)throw std::runtime_error("Unsupported host protocol");
        Bytes hello{uint8_t(mirror),uint8_t(count)};for(int v:views){if(v<0||v>3)throw std::runtime_error("Invalid view");hello.push_back(uint8_t(v));}
        data.insert(data.end(),hello.begin(),hello.end());auto proof=mac(token,data);hello.insert(hello.end(),proof.begin(),proof.end());
        if(!send_message(s,Hello,hello)||!receive_message(s,type,data)||type!=Welcome||data.size()!=2||data[1]!=count||data[0]+count>4)
            throw std::runtime_error("Session rejected: check key and free player slots");
        impl->count=count;impl->first=data[0];return true;
    } catch(const std::exception& e){error=e.what();close();return false;}
}
bool Client::send_input(const std::array<Pad,4>& pads){Bytes b;for(int i=0;i<impl->count;++i){put16(b,pads[i].buttons);b.push_back(uint8_t(pads[i].x));b.push_back(uint8_t(pads[i].y));}std::lock_guard lock(impl->send_mutex);return send_message(impl->socket,Input,b);}
bool Client::send_control(int command){std::lock_guard lock(impl->send_mutex);return send_message(impl->socket,Control,Bytes{uint8_t(command)});}
bool Client::receive_image(Image& image){
    Type type{};Bytes b;
    while(receive_message(impl->socket,type,b)){
        if(type==Audio && b.size()>=4 && b.size()<=262148 && (b.size()-4)%4==0){int rate=int(get32(b.data()));if(rate<8000||rate>96000)return false;
            std::lock_guard lock(impl->audio_mutex);if(impl->rate!=rate||impl->pcm.size()+b.size()-4>size_t(rate))impl->pcm.clear();impl->rate=rate;impl->pcm.insert(impl->pcm.end(),b.begin()+4,b.end());continue;}
        if(type!=Frame||b.size()<6||b[0]>=impl->count)return false;
        int w=get16(b.data()+1),h=get16(b.data()+3),actual_w=0,actual_h=0,channels=0;
        if(w<1||h<1||w>2560||h>1440||!stbi_info_from_memory(b.data()+5,int(b.size()-5),&actual_w,&actual_h,&channels)||w!=actual_w||h!=actual_h)return false;
        auto* rgb=stbi_load_from_memory(b.data()+5,int(b.size()-5),&actual_w,&actual_h,&channels,3);if(!rgb)return false;
        image.player=b[0];image.width=w;image.height=h;image.rgb.assign(rgb,rgb+size_t(w)*h*3);stbi_image_free(rgb);return true;
    }return false;
}
bool Client::take_audio(std::vector<uint8_t>& pcm,int& rate){std::lock_guard lock(impl->audio_mutex);pcm.swap(impl->pcm);rate=impl->rate;return !pcm.empty();}

// No SDL/window/device access: exercised by three independent OS processes.
int game_peer_test() {
    const char* key=std::getenv("BAR_NET_TOKEN");if(!key)return 1;
    Client client;std::string error;bool connected=false;
    for(int n=0;n<200&&!connected;++n){connected=client.connect("127.0.0.1",28768,key,1,false,{0,1,2,3},error);if(!connected)Sleep(50);}
    if(!connected||client.first_port()!=2){std::fprintf(stderr,"FAIL: game peer connection: %s\n",error.c_str());return 1;}
    std::atomic<bool> sending=true;
    std::thread sender([&]{std::array<Pad,4> pads{};pads[0]={0x8000,0,0};while(sending){if(!client.send_input(pads))break;Sleep(16);}});
    Image frame;int frames=0;uint64_t start=now();
    while(now()-start<150000&&client.receive_image(frame)) ++frames;
    sending=false;client.close();sender.join();
    std::printf("%s: game peer slot=2 received_frames=%d elapsed_ms=%llu\n",frames>60?"PASS":"FAIL",frames,(unsigned long long)(now()-start));
    return frames>60?0:1;
}
int menu_client_test() {
    const char* token=std::getenv("BAR_NET_TOKEN");if(!token)return 1;
    Client client;std::string error;
    if(!client.connect("127.0.0.1",28769,token,2,true,{0,1,2,3},error))return 1;
    std::atomic<bool> sending=true;
    std::thread sender([&]{while(sending){if(!client.send_input({}))break;Sleep(16);}});
    bool seen[2]{};Image frame;uint64_t deadline=now()+12000;
    while(now()<deadline&&client.receive_image(frame)) {
        if(frame.player<2&&frame.width>0&&frame.height>0)seen[frame.player]=true;
        if(seen[0]&&seen[1])break;
    }
    sending=false;client.close();sender.join();
    const bool ok=seen[0]&&seen[1];std::puts(ok?"PASS: menu client received both views":"FAIL: menu client frames");return ok?0:1;
}

int transport_host_test() {
    _putenv_s("BAR_NET_HOST_PORT","28767");_putenv_s("BAR_NET_BIND","127.0.0.1");
    _putenv_s("BAR_NET_LOCAL_PLAYERS","2");_putenv_s("BAR_VIEW_ACTIVE","1");_putenv_s("BAR_VIEW_LAYOUT","3");
    std::string error;
    if (!start_host_from_environment(error)) { std::fprintf(stderr,"FAIL: %s\n",error.c_str()); return 1; }
    Bytes pixels(16*16*4,0);
    for(int y=0;y<16;++y)for(int x=0;x<16;++x){size_t p=(y*16+x)*4;pixels[p+(y<8?(x<8?2:0):1)]=255;pixels[p+3]=255;}
    bool seen[3]{}; uint64_t deadline=now()+12000;
    while(now()<deadline) {
        for(int i=0;i<3;++i){Pad p{};if(read_pad(i,p)&&p.buttons==uint16_t(0x8000>>i)&&p.x==10+i)seen[i]=true;}
        submit_bgra(pixels.data(),16,16,64);
        int16_t pcm[]{-123,123};submit_audio(pcm,2,32000);
        Sleep(25);
        if(seen[0]&&seen[1]&&seen[2]) {
            // Leave peers enough time to receive their own image/audio before closing.
            for(int n=0;n<20;++n){submit_bgra(pixels.data(),16,16,64);submit_audio(pcm,2,32000);Sleep(25);}
            stop_host();std::puts("PASS: two local inputs plus one remote input, separate processes");return 0;
        }
    }
    stop_host();std::fputs("FAIL: did not receive all three independent input slots\n",stderr);return 1;
}
int transport_peer_test(bool mirror) {
    const char* key=std::getenv("BAR_NET_TOKEN");if(!key)return 1;
    Client client;std::string error;bool connected=false;
    for(int n=0;n<40&&!connected;++n){connected=client.connect("127.0.0.1",28767,key,mirror?2:1,mirror,{0,1,2,3},error);if(!connected)Sleep(50);}
    if(!connected){std::fprintf(stderr,"FAIL: %s\n",error.c_str());return 1;}
    const int first=client.first_port(), count=mirror?2:1;
    if(first!=(mirror?0:2)){client.close();return 1;}
    std::atomic<bool> sending=true;
    std::thread sender([&]{std::array<Pad,4> pads{};for(int i=0;i<count;++i)pads[i]={uint16_t(0x8000>>(first+i)),int8_t(10+first+i),0};while(sending){if(!client.send_input(pads))break;Sleep(16);}});
    bool seen[2]{},audio=false,ok=false;Image frame;uint64_t deadline=now()+8000;
    while(now()<deadline&&client.receive_image(frame)) {
        if(frame.player<count&&frame.width==8&&frame.height==8){int channel=mirror?(frame.player==0?0:2):1;seen[frame.player]=frame.rgb[channel]>200;}
        Bytes pcm;int rate=0;if(client.take_audio(pcm,rate))audio=rate==32000&&pcm.size()>=4;
        ok=seen[0]&&(!mirror||seen[1])&&audio;if(ok)break;
    }
    // Keep sending after rendering checks, until the host's routing checks have completed.
    if(ok)Sleep(250);
    sending=false;client.close();sender.join();
    std::printf("%s: %s slots, distinct player video and PCM\n",ok?"PASS":"FAIL",mirror?"local 1+2":"remote 3");return ok?0:1;
}

int self_test(){
    auto check=[](bool ok,const char* what){if(!ok)throw std::runtime_error(what);};
    try{
        for(int layout=1;layout<=3;++layout){int area=0,n=layout==3?4:2;for(int i=0;i<n;++i){auto r=view_rect(layout,i,17,13);check(r.x>=0&&r.y>=0&&r.x+r.width<=17&&r.y+r.height<=13,"View bounds");area+=r.width*r.height;}check(area==17*13,"View coverage");}
        std::string token=make_token(),error;
        _putenv_s("BAR_NET_HOST_PORT","28766");_putenv_s("BAR_NET_BIND","127.0.0.1");_putenv_s("BAR_NET_LOCAL_PLAYERS","2");_putenv_s("BAR_NET_TOKEN",token.c_str());_putenv_s("BAR_VIEW_ACTIVE","1");_putenv_s("BAR_VIEW_LAYOUT","3");
        check(start_host_from_environment(error),error.c_str());
        Client mirror,remote,bad,extra,overflow;std::array<int,4> views{0,1,2,3};
        check(!bad.connect("127.0.0.1",28766,make_token(),1,false,views,error),"Wrong key accepted");
        check(mirror.connect("127.0.0.1",28766,token,2,true,views,error),error.c_str());
        check(remote.connect("127.0.0.1",28766,token,1,false,views,error),error.c_str());check(remote.first_port()==2,"Remote slot overlaps local players");
        check(extra.connect("127.0.0.1",28766,token,1,false,views,error)&&extra.first_port()==3,"Second remote slot");
        check(!overflow.connect("127.0.0.1",28766,token,1,false,views,error),"Fifth player accepted");
        std::array<Pad,4> pads{};pads[0]={0x8000,42,-33};check(remote.send_input(pads),"Send input");
        Pad received;uint64_t input_deadline=now()+1000;
        while(now()<input_deadline){if(read_pad(2,received)&&received.buttons==0x8000)break;Sleep(5);}
        check(received.buttons==0x8000&&received.x==42&&received.y==-33,"Input routing");
        check(port_connected(2)&&port_connected(3),"Connected ports");
        Bytes pixels(16*16*4);for(int y=0;y<16;++y)for(int x=0;x<16;++x){size_t i=(y*16+x)*4;pixels[i+1]=y>=8&&x<8?255:0;pixels[i+2]=y<8?255:0;pixels[i+3]=255;}
        int16_t sound[]{-1000,1000,-2000,2000};submit_audio(sound,4,32000);
        submit_bgra(pixels.data(),16,16,64);Image image;check(remote.receive_image(image),"JPEG frame transport");check(image.width==8&&image.height==8&&image.rgb[1]>200,"Wrong player view");
        submit_bgra(pixels.data(),16,16,64);check(remote.receive_image(image),"Second frame transport");
        Bytes pcm;int rate=0;check(remote.take_audio(pcm,rate)&&rate==32000&&pcm.size()==sizeof(sound)&&!std::memcmp(pcm.data(),sound,sizeof(sound)),"PCM audio transport");
        Sleep(550);check(read_pad(2,received)&&received.buttons==0&&received.x==0,"Stale input not released");
        remote.close();mirror.close();extra.close();uint64_t disconnect_deadline=now()+1000;
        while(port_connected(2)&&now()<disconnect_deadline)Sleep(5);
        check(!port_connected(2),"Disconnected player remained connected");
        stop_host();std::puts("PASS: authentication, four-player capacity, slot isolation, JPEG player view, PCM audio, stale inputs, disconnect, view rectangles");return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());stop_host();return 1;}
}
}
