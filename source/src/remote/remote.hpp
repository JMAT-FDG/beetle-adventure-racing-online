#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bar::remote {
struct Pad { uint16_t buttons = 0; int8_t x = 0, y = 0; };
struct Image { int player = 0, width = 0, height = 0; std::vector<uint8_t> rgb; };
struct Rect { int x, y, width, height; };
// 0=full frame, 1=top/bottom, 2=left/right, 3=four quadrants.
Rect view_rect(int layout, int player, int width, int height);
bool start_host_from_environment(std::string& error);
void stop_host();
bool hosting();
int local_players();
bool port_connected(int port);
bool read_pad(int port, Pad& pad); // false leaves local input in charge
bool views_active();
bool local_viewer_connected();
void control(int command);       // 0=toggle views, 1=cycle layout
void render_size(int& width, int& height);
bool frame_wanted();
void submit_bgra(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t pitch);
void submit_audio(const int16_t* samples, size_t count, uint32_t rate);

class Client {
public:
    Client();
    ~Client();
    bool connect(const std::string& address, uint16_t port, const std::string& token,
                 int count, bool local_mirror, const std::array<int,4>& views, std::string& error);
    bool send_input(const std::array<Pad,4>& pads);
    bool send_control(int command);
    bool receive_image(Image& image); // blocking, also queues bounded audio
    bool take_audio(std::vector<uint8_t>& pcm, int& rate);
    int first_port() const;
    void close();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
std::string make_token();
int self_test();
int transport_host_test();
int transport_peer_test(bool mirror);
int game_peer_test();
int menu_client_test();
}
