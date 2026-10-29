#include "bar_online_menu.h"
#include "remote/remote.hpp"
#include "recompui/recompui.h"
#include "elements/ui_modal.h"
#include "elements/ui_label.h"
#include "elements/ui_text_input.h"
#include <SDL.h>
#include <windows.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>
#include "ultramodern/ultramodern.hpp"
extern "C" void bar_rt64_request_screenshot(const char*);

namespace bar::online_menu {
namespace {
using namespace recompui;
struct Request { bool host; int locals,racers,screen1,screen2; std::string address,key; };
std::mutex request_mutex;
std::optional<Request> pending;
std::string result;
HANDLE viewer=nullptr;
Modal* panel=nullptr;
TextInput *address=nullptr,*key=nullptr;
Label* status=nullptr;
int locals=2,racers=3,screen1=0,screen2=1,displays=1;
Button *local_button=nullptr,*race_button=nullptr,*screen_buttons[2]{};
class OnlineUpdater : public Element {
    void process_event(const Event& event) override {if(event.type==EventType::Update)bar::online_menu::update();}
public:
    OnlineUpdater(ResourceId id,Element* parent):Element(id,parent,Events(EventType::Update),"div",false){set_height(0);}
};
void labels() {
    local_button->set_text("Joueurs sur ce PC : "+std::to_string(locals));
    race_button->set_text("Pilotes en course : "+std::to_string(racers)+(racers==3?" (experimental)":""));
    for(int i=0;i<2;++i) {
        int index=i?screen2:screen1;SDL_Rect bounds{};SDL_GetDisplayBounds(index,&bounds);
        screen_buttons[i]->set_text("J"+std::to_string(i+1)+" : ecran "+std::to_string(index+1)+" - "+
            std::to_string(bounds.w)+" x "+std::to_string(bounds.h));
        screen_buttons[i]->set_enabled(i==0||locals==2);
    }
}
bool valid_key(const std::string& value) {
    return value.size()==32&&std::all_of(value.begin(),value.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');});
}
void enqueue(bool host) {
    auto token=key->get_text(),host_address=address->get_text();
    if(!valid_key(token)){status->set_text("Le code doit contenir 32 caracteres hexadecimaux.");return;}
    if(!host&&(host_address.empty()||host_address.size()>253||!std::all_of(host_address.begin(),host_address.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='.'||c=='-';}))) {
        status->set_text("Saisis l'adresse IP ou le nom du PC hote.");return;
    }
    if(locals==2&&screen1==screen2&&displays>1){status->set_text("Choisis deux ecrans differents.");return;}
    std::lock_guard lock(request_mutex);
    pending=Request{host,locals,racers,screen1,screen2,host_address,token};
    result="Preparation de la connexion...";
}
Button* button(ContextId context,Element* parent,const std::string& text,std::function<void()> action) {
    auto* b=context.create_element<Button>(parent,text,ButtonStyle::Secondary,ButtonSize::Small);
    b->add_pressed_callback(std::move(action));return b;
}
void build() {
    auto previous=get_current_context();
    if(previous!=ContextId::null())previous.close();
    panel=Modal::create_modal();auto context=panel->modal_root_context;context.open();
    auto* body=panel->get_body();body->set_display(Display::Flex);body->set_flex_direction(FlexDirection::Column);
    context.create_element<OnlineUpdater>(body);
    body->set_gap(10);body->set_padding(20);
    context.create_element<Label>(panel->get_header()->get_left(),"Multijoueur en ligne",theme::Typography::Header3);
    context.create_element<Label>(body,"Heberge une partie ou rejoins un ami depuis ce meme menu.",LabelStyle::Normal);
    context.create_element<Label>(body,"Adresse du PC hote (pour rejoindre)",LabelStyle::Small);
    address=context.create_element<TextInput>(body);address->set_text("127.0.0.1");address->set_height(38);
    context.create_element<Label>(body,"Code de la partie",LabelStyle::Small);
    key=context.create_element<TextInput>(body);key->set_height(38);
    const char* existing=std::getenv("BAR_NET_TOKEN");key->set_text(existing?existing:remote::make_token());
    button(context,body,"Copier le code",[]{SDL_SetClipboardText(key->get_text().c_str());});
    displays=std::max(1,SDL_GetNumVideoDisplays());locals=displays>1?2:1;
    screen1=displays>=3?1:0;screen2=displays>=3?2:(displays>1?1:0);
    local_button=button(context,body,"",[]{locals=locals==1?2:1;labels();});
    race_button=button(context,body,"",[]{racers=racers==2?3:2;labels();});
    for(int i=0;i<2;++i)screen_buttons[i]=button(context,body,"",[i]{int& index=i?screen2:screen1;index=(index+1)%displays;labels();});
    button(context,body,"Heberger",[]{enqueue(true);});
    button(context,body,"Rejoindre",[]{enqueue(false);});
    status=context.create_element<Label>(body,"A 3 : deux ici + un ami. Rejoindre avant Start Game, puis Single Race. F9 separe les vues.",LabelStyle::Small);
    button(context,body,"Retour au jeu",[]{panel->close();});
    panel->set_menu_action_callback(MenuAction::Back,[]{panel->close();},"Retour");
    labels();context.close();if(previous!=ContextId::null())previous.open();
    std::fprintf(stderr,"[online-menu] built: host/join, code, local players and monitor controls\n");
}
void env(const char* name,const std::string& value){_putenv_s(name,value.c_str());}
void launch(const Request& request) {
    if(viewer)throw std::runtime_error("Les vues sont deja ouvertes. Ferme-les avec Echap avant de relancer.");
    if(!request.host&&remote::hosting())throw std::runtime_error("Une partie est deja hebergee. Relance le jeu pour rejoindre un autre hote.");
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    const auto client=std::filesystem::path(executable).parent_path()/"beetle-remote.exe";
    if(!std::filesystem::exists(client))throw std::runtime_error("beetle-remote.exe est introuvable pres du jeu.");
    if(request.host&&!remote::hosting()) {
        if(ultramodern::is_game_started())throw std::runtime_error("Configure l'hebergement avant Start Game. Relance le jeu pour preparer une nouvelle partie.");
        const bool test=std::getenv("BAR_BACKGROUND_TEST")&&std::getenv("BAR_TEST_ONLINE_SESSION");
        env("BAR_NET_TOKEN",request.key);env("BAR_NET_HOST_PORT",test?"28769":"28765");env("BAR_NET_BIND",test?"127.0.0.1":"0.0.0.0");
        env("BAR_NET_LOCAL_PLAYERS",std::to_string(request.locals));env("BAR_SEPARATE_KEYBOARD",request.locals==2?"1":"0");
        env("BAR_RACE_PLAYERS",std::to_string(request.racers));env("BAR_VIEW_LAYOUT",request.racers==3?"3":"1");
        env("BAR_VIEW_ACTIVE","0");env("BAR_NET_FPS","30");env("BAR_NET_QUALITY","90");
        SDL_Rect a{},b{};SDL_GetDisplayBounds(request.screen1,&a);SDL_GetDisplayBounds(request.screen2,&b);
        env("BAR_VIEW_WIDTH",std::to_string(std::clamp(std::max(a.w,request.locals==2?b.w:0),320,2560)));
        env("BAR_VIEW_HEIGHT",std::to_string(std::clamp(std::max(a.h,request.locals==2?b.h:0),240,1440)));
        std::string error;if(!remote::start_host_from_environment(error))throw std::runtime_error(error);
    }
    // Pass credentials only through a private child environment, not its command line.
    std::wstring block;std::vector<std::wstring> entries;const wchar_t* variables=GetEnvironmentStringsW();
    if(!variables)throw std::runtime_error("Impossible de preparer le client.");
    for(auto* p=variables;*p;p+=wcslen(p)+1) {
        if(_wcsnicmp(p,L"BAR_NET_TOKEN=",14)==0)continue;
        entries.emplace_back(p);
    }
    FreeEnvironmentStringsW(const_cast<wchar_t*>(variables));
    entries.push_back(L"BAR_NET_TOKEN="+std::wstring(request.key.begin(),request.key.end()));
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return _wcsicmp(a.c_str(),b.c_str())<0;});
    for(const auto& entry:entries){block+=entry;block.push_back(0);}block.push_back(0);
    std::string arguments=" --no-dialog --host "+(request.host?"127.0.0.1":request.address)+" --players "+std::to_string(request.locals)+" --displays "+std::to_string(request.screen1);
    if(request.locals==2)arguments+=","+std::to_string(request.screen2);
    if(request.host)arguments+=" --mirror";
    if(displays==1&&request.locals==2)arguments+=" --windowed";
    if(std::getenv("BAR_BACKGROUND_TEST")&&std::getenv("BAR_TEST_ONLINE_SESSION"))arguments=" --menu-client-test";
    std::wstring command=L"\""+client.wstring()+L"\""+std::wstring(arguments.begin(),arguments.end());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(client.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_UNICODE_ENVIRONMENT|CREATE_NO_WINDOW,block.data(),client.parent_path().c_str(),&startup,&process))
        throw std::runtime_error("Impossible de lancer les vues du client.");
    CloseHandle(process.hThread);viewer=process.hProcess;
}
}
void open(){if(!panel)build();panel->open();}
void update(){
    if(!panel)return;
    if(get_current_context()!=panel->modal_root_context)return;
    static bool first=true;if(first){first=false;std::fprintf(stderr,"[online-menu] update active\n");std::fflush(stderr);}
    if(std::getenv("BAR_TEST_ONLINE_MENU") && std::getenv("BAR_BACKGROUND_TEST")) {
        static int frames=0;
        if(++frames==20){if(const char* path=std::getenv("BAR_TEST_ONLINE_SHOT"))bar_rt64_request_screenshot(path);}
    }
    std::lock_guard lock(request_mutex);
    if(!result.empty()){status->set_text(result);result.clear();}
    if(remote::hosting()){key->set_enabled(false);local_button->set_enabled(false);race_button->set_enabled(false);}
}
void test_panel(){
    open();
    auto previous=get_current_context();if(previous!=ContextId::null())previous.close();panel->modal_root_context.open();
    auto require=[](bool ok){if(!ok)throw std::runtime_error("Online panel test failed");};
    const auto saved=key->get_text();require(valid_key(saved));
    key->set_text("invalid");enqueue(true);
    {std::lock_guard lock(request_mutex);require(!pending);}
    key->set_text(saved);address->set_text("bad address");enqueue(false);
    {std::lock_guard lock(request_mutex);require(!pending);}
    address->set_text("127.0.0.1");enqueue(true);
    {std::lock_guard lock(request_mutex);require(pending&&pending->host&&pending->racers==3);pending.reset();result.clear();}
    locals=1;labels();enqueue(false);
    {std::lock_guard lock(request_mutex);require(pending&&!pending->host&&pending->locals==1);pending.reset();result.clear();}
    locals=displays>1?2:1;labels();panel->close();open();
    require(panel->is_open_now());key->set_text("0123456789abcdef0123456789abcdef");
    status->set_text("Test du menu : Heberger / Rejoindre, champs et choix des ecrans verifies.");
    if(const char* path=std::getenv("BAR_TEST_ONLINE_SHOT"))bar_rt64_request_screenshot(path);
    std::fprintf(stderr,"PASS: integrated online panel, host/join requests, input validation, reopen\n");std::fflush(stderr);
    if(std::getenv("BAR_TEST_ONLINE_SESSION"))enqueue(true);
    panel->modal_root_context.close();if(previous!=ContextId::null())previous.open();
}
void pump(){
    if(std::getenv("BAR_TEST_ONLINE_MENU")&&std::getenv("BAR_BACKGROUND_TEST")&&!std::getenv("BAR_TEST_ONLINE_SESSION"))return;
    std::optional<Request> request;
    {std::lock_guard lock(request_mutex);request.swap(pending);}
    if(viewer&&WaitForSingleObject(viewer,0)==WAIT_OBJECT_0){DWORD code=0;GetExitCodeProcess(viewer,&code);CloseHandle(viewer);viewer=nullptr;
        if(std::getenv("BAR_TEST_ONLINE_SESSION")){std::fprintf(stderr,"%s: integrated menu client process exit=%lu\n",code?"FAIL":"PASS",(unsigned long)code);std::fflush(stderr);}
        std::lock_guard lock(request_mutex);result=code?"Connexion impossible. Verifie l'adresse, le code et que l'hote est disponible.":"Vues fermees. Tu peux rejoindre a nouveau.";}
    if(!request)return;
    std::string message;
    try{launch(*request);message=request->host?"Partie hebergee. Donne le code a ton ami avant Start Game. Puis Single Race et F9.":"Connexion au PC hote... Les vues vont s'ouvrir sur les ecrans choisis.";}
    catch(const std::exception& error){message=error.what();}
    std::lock_guard lock(request_mutex);result=message;
}
}
