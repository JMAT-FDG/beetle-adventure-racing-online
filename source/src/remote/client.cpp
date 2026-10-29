#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "remote.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {
struct View {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;SDL_Texture* texture=nullptr;
    int width=0,height=0;
    ~View(){if(texture)SDL_DestroyTexture(texture);if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);}
};
bar::remote::Pad keyboard() {
    const auto* k=SDL_GetKeyboardState(nullptr);bar::remote::Pad p;
    auto set=[&](bool on,uint16_t bit){if(on)p.buttons|=bit;};
    set(k[SDL_SCANCODE_SPACE]||k[SDL_SCANCODE_X],0x8000);set(k[SDL_SCANCODE_LSHIFT]||k[SDL_SCANCODE_Z],0x4000);
    set(k[SDL_SCANCODE_RETURN],0x1000);set(k[SDL_SCANCODE_LCTRL],0x2000);set(k[SDL_SCANCODE_Q],0x20);set(k[SDL_SCANCODE_E],0x10);
    set(k[SDL_SCANCODE_I],8);set(k[SDL_SCANCODE_K],4);set(k[SDL_SCANCODE_J],2);set(k[SDL_SCANCODE_L],1);
    p.x=int8_t(80*(int(k[SDL_SCANCODE_RIGHT]||k[SDL_SCANCODE_D])-int(k[SDL_SCANCODE_LEFT]||k[SDL_SCANCODE_A])));
    p.y=int8_t(80*(int(k[SDL_SCANCODE_UP]||k[SDL_SCANCODE_W])-int(k[SDL_SCANCODE_DOWN]||k[SDL_SCANCODE_S])));
    return p;
}
bar::remote::Pad gamepad(SDL_GameController* c) {
    bar::remote::Pad p;auto b=[&](SDL_GameControllerButton button){return SDL_GameControllerGetButton(c,button)!=0;};
    auto set=[&](bool on,uint16_t bit){if(on)p.buttons|=bit;};
    set(b(SDL_CONTROLLER_BUTTON_A),0x8000);set(b(SDL_CONTROLLER_BUTTON_X),0x4000);set(b(SDL_CONTROLLER_BUTTON_START),0x1000);
    set(SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>8192,0x2000);
    set(b(SDL_CONTROLLER_BUTTON_LEFTSHOULDER),0x20);set(SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>8192,0x10);
    set(b(SDL_CONTROLLER_BUTTON_DPAD_UP),0x800);set(b(SDL_CONTROLLER_BUTTON_DPAD_DOWN),0x400);
    set(b(SDL_CONTROLLER_BUTTON_DPAD_LEFT),0x200);set(b(SDL_CONTROLLER_BUTTON_DPAD_RIGHT),0x100);
    auto rx=SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_RIGHTX),ry=SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_RIGHTY);
    set(rx>12000||b(SDL_CONTROLLER_BUTTON_B),1);set(rx<-12000||b(SDL_CONTROLLER_BUTTON_Y),2);set(ry>12000,4);set(ry<-12000,8);
    auto axis=[](int v){return int8_t(std::abs(v)<5000?0:std::clamp(v*80/32767,-80,80));};
    p.x=axis(SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_LEFTX));p.y=axis(-int(SDL_GameControllerGetAxis(c,SDL_CONTROLLER_AXIS_LEFTY)));
    return p;
}
}

int main(int argc,char** argv) {
    std::string address="127.0.0.1",key,error;int port=28765,count=1;bool mirror=false,windowed=false,list=false,probe=false,no_dialog=false;
    std::vector<int> displays;
    try {
        for(int i=1;i<argc;++i) {
            std::string a=argv[i];auto value=[&](){if(++i>=argc)throw std::runtime_error("Missing option value");return std::string(argv[i]);};
            if(a=="--self-test")return bar::remote::self_test();
            if(a=="--transport-host-test")return bar::remote::transport_host_test();
            if(a=="--transport-local-test")return bar::remote::transport_peer_test(true);
            if(a=="--transport-peer-test")return bar::remote::transport_peer_test(false);
            if(a=="--game-peer-test")return bar::remote::game_peer_test();
            if(a=="--menu-client-test")return bar::remote::menu_client_test();
            if(a=="--new-key"){std::puts(bar::remote::make_token().c_str());return 0;}
            if(a=="--host")address=value();else if(a=="--key")key=value();else if(a=="--port")port=std::stoi(value());
            else if(a=="--players")count=std::stoi(value());else if(a=="--mirror")mirror=true;
            else if(a=="--windowed")windowed=true;else if(a=="--list-displays")list=true;
            else if(a=="--probe")probe=true;
            else if(a=="--no-dialog")no_dialog=true;
            else if(a=="--displays") {auto s=value();size_t at=0;while(at<s.size()){size_t next=s.find(',',at);displays.push_back(std::stoi(s.substr(at,next-at)));if(next==std::string::npos)break;at=next+1;}}
            else throw std::runtime_error("Unknown option: "+a);
        }
        if(count<1||count>4||port<1024||port>65535)throw std::runtime_error("Invalid player count or port");
        if(key.empty()){const char* p=std::getenv("BAR_NET_TOKEN");if(p)key=p;}
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS,"permonitorv2");SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER))throw std::runtime_error(SDL_GetError());
        int total=SDL_GetNumVideoDisplays();
        if(list){for(int i=0;i<total;++i){SDL_Rect r{};SDL_GetDisplayBounds(i,&r);std::printf("%d: %s %dx%d at %d,%d\n",i,SDL_GetDisplayName(i),r.w,r.h,r.x,r.y);}SDL_Quit();return 0;}
        if(displays.empty())for(int i=0;i<count;++i)displays.push_back((mirror&&total>count?i+1:i)%total);
        if(displays.size()!=size_t(count))throw std::runtime_error("Choose one display per player");
        std::vector<int> unique=displays;std::sort(unique.begin(),unique.end());
        if(!windowed&&std::adjacent_find(unique.begin(),unique.end())!=unique.end())throw std::runtime_error("A full-screen display cannot be assigned twice");
        for(int d:displays)if(d<0||d>=total)throw std::runtime_error("Display index out of range");
        bar::remote::Client client;
        if(!client.connect(address,uint16_t(port),key,count,mirror,{0,1,2,3},error))throw std::runtime_error(error);
        if(probe) {
            if(!mirror)throw std::runtime_error("The render probe requires a local mirror connection");
            std::atomic<bool> sending=true;
            std::thread keepalive([&]{while(sending){client.send_input({});SDL_Delay(16);}});
            bool ok=false;std::array<bool,4> seen{};
            client.send_control(0);
            uint64_t deadline=SDL_GetTicks64()+15000;bar::remote::Image image;
            while(SDL_GetTicks64()<deadline && client.receive_image(image)) {
                auto extrema=std::minmax_element(image.rgb.begin(),image.rgb.end());
                if(image.width==1920&&image.height==1080&&*extrema.second-*extrema.first>10)seen[image.player]=true;
                ok=std::all_of(seen.begin(),seen.begin()+count,[](bool value){return value;});if(ok)break;
            }
            client.send_control(0);sending=false;client.close();keepalive.join();SDL_Quit();
            std::printf("%s: live GPU frames received for %d views at 1920x1080\n",ok?"PASS":"FAIL",count);return ok?0:1;
        }
        std::array<std::unique_ptr<View>,4> views;
        for(int i=0;i<count;++i){SDL_Rect r{};SDL_GetDisplayBounds(displays[i],&r);auto v=std::make_unique<View>();
            std::string title="Beetle - Player "+std::to_string(client.first_port()+i+1)+" | F9 views, F8 layout, Esc quit";
            v->window=SDL_CreateWindow(title.c_str(),r.x+(windowed?40*i:0),r.y+(windowed?40*i:0),windowed?960:r.w,windowed?540:r.h,
                SDL_WINDOW_ALLOW_HIGHDPI|(windowed?SDL_WINDOW_RESIZABLE:SDL_WINDOW_BORDERLESS));
            if(!v->window)throw std::runtime_error(SDL_GetError());
            v->renderer=SDL_CreateRenderer(v->window,-1,SDL_RENDERER_ACCELERATED);
            if(!v->renderer)v->renderer=SDL_CreateRenderer(v->window,-1,SDL_RENDERER_SOFTWARE);
            if(!v->renderer)throw std::runtime_error(SDL_GetError());views[i]=std::move(v);
        }
        std::atomic<bool> running=true;std::mutex images_mutex;std::array<std::optional<bar::remote::Image>,4> images;
        std::thread receiver([&]{bar::remote::Image image;while(running&&client.receive_image(image)){std::lock_guard lock(images_mutex);images[image.player]=std::move(image);}running=false;});
        SDL_AudioDeviceID audio=0;int audio_rate=0;std::vector<SDL_GameController*> pads;bool rescan=true;
        while(running){uint64_t began=SDL_GetTicks64();SDL_Event event;
            while(SDL_PollEvent(&event)){
                if(event.type==SDL_QUIT||(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_CLOSE))running=false;
                if(event.type==SDL_KEYDOWN&&!event.key.repeat){
                    if(event.key.keysym.scancode==SDL_SCANCODE_ESCAPE)running=false;
                    if((mirror||client.first_port()==0)&&event.key.keysym.scancode==SDL_SCANCODE_F9)client.send_control(0);
                    if((mirror||client.first_port()==0)&&event.key.keysym.scancode==SDL_SCANCODE_F8)client.send_control(1);
                }
                if(event.type==SDL_CONTROLLERDEVICEADDED||event.type==SDL_CONTROLLERDEVICEREMOVED)rescan=true;
            }
            if(rescan){for(auto* p:pads)SDL_GameControllerClose(p);pads.clear();for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){auto* p=SDL_GameControllerOpen(i);if(p)pads.push_back(p);}rescan=false;}
            std::array<bar::remote::Pad,4> input{};
            if(SDL_GetKeyboardFocus()) {
                input[0]=keyboard();
                for(size_t i=0;i<pads.size();++i){int slot=count==1?0:int(i)+1;if(slot>=count)break;auto p=gamepad(pads[i]);input[slot].buttons|=p.buttons;
                    if(p.x||p.y){input[slot].x=p.x;input[slot].y=p.y;}}
            }
            if(!client.send_input(input))running=false;
            std::vector<uint8_t> pcm;int rate=0;
            if(client.take_audio(pcm,rate)){
                if(rate!=audio_rate){if(audio)SDL_CloseAudioDevice(audio);SDL_AudioSpec want{},have{};want.freq=rate;want.channels=2;want.format=AUDIO_S16LSB;want.samples=1024;
                    audio=SDL_OpenAudioDevice(nullptr,0,&want,&have,0);audio_rate=rate;if(audio)SDL_PauseAudioDevice(audio,0);}
                if(audio){if(SDL_GetQueuedAudioSize(audio)>uint32_t(rate))SDL_ClearQueuedAudio(audio);SDL_QueueAudio(audio,pcm.data(),uint32_t(pcm.size()));}
            }
            for(int i=0;i<count;++i){std::optional<bar::remote::Image> image;{std::lock_guard lock(images_mutex);image.swap(images[i]);}auto& v=*views[i];
                if(image){if(v.width!=image->width||v.height!=image->height){if(v.texture)SDL_DestroyTexture(v.texture);v.width=image->width;v.height=image->height;
                    v.texture=SDL_CreateTexture(v.renderer,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,v.width,v.height);}
                    if(v.texture)SDL_UpdateTexture(v.texture,nullptr,image->rgb.data(),v.width*3);}
                SDL_SetRenderDrawColor(v.renderer,0,0,0,255);SDL_RenderClear(v.renderer);
                if(v.texture){int w,h;SDL_GetRendererOutputSize(v.renderer,&w,&h);double scale=std::min(double(w)/v.width,double(h)/v.height);
                    SDL_Rect dst{int((w-v.width*scale)/2),int((h-v.height*scale)/2),int(v.width*scale),int(v.height*scale)};SDL_RenderCopy(v.renderer,v.texture,nullptr,&dst);}
                SDL_RenderPresent(v.renderer);
            }
            auto elapsed=SDL_GetTicks64()-began;if(elapsed<16)SDL_Delay(uint32_t(16-elapsed));
        }
        client.close();receiver.join();if(audio)SDL_CloseAudioDevice(audio);for(auto* p:pads)SDL_GameControllerClose(p);for(auto& v:views)v.reset();SDL_Quit();return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());if(!probe&&!no_dialog)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Beetle Remote",e.what(),nullptr);SDL_Quit();return 1;}
}
