#pragma once

#include <map>
#include <string>
#include "esphome/core/component.h"
#include "esphome/components/display/display.h"
#include "esphome/components/font/font.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/http_request/http_request.h"
#include "esphome/components/image/image.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace soccer_tracker {

enum MatchState {
  SCHEDULED,      // Match is scheduled but not today
  TODAY_PENDING,  // Match is today but hasn't started
  IN_PROGRESS,    // Match is currently being played
  FINISHED        // Match just finished (show for 1 hour)
};

struct Team {
  std::string name;
  std::string logo_id;
  int score;
};

struct Match {
  Team home_team;
  Team away_team;
  time_t match_time;
  MatchState state;
  int minute;      // Current minute of play
  int second;      // Current second within minute
  time_t finish_time; // Time when match finished (for FINISHED state)
};

class SoccerTracker : public Component {
  public:
    void setup() override;
      // Runtime test controls
      void set_test_mode(bool enabled) { this->test_mode_ = enabled; }
      void set_test_server_url(const std::string &url) { this->test_server_url_ = url; }
      bool get_test_mode() const { return this->test_mode_; }
      const std::string &get_test_server_url() const { return this->test_server_url_; }
    void loop() override;
    void dump_config() override;
    
    float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
    
    void draw_match();
    
    void set_display(display::Display *display) { display_ = display; }
    void set_font(font::Font *font) { font_ = font; }
    void set_small_font(font::Font *font) { small_font_ = font; }
    void set_rtc(time::RealTimeClock *rtc) { rtc_ = rtc; }
    void set_http_request(http_request::HttpRequestComponent *http_request) { http_request_ = http_request; }
    
    void set_api_key(const std::string &api_key) { api_key_ = api_key; }
    void set_favorite_team(const std::string &team) { favorite_team_ = team; }
    void set_team_id(int team_id) { team_id_ = team_id; }
    
    void register_team_logo(const std::string &team_name, image::Image *logo) {
      team_logos_[team_name] = logo;
    }
    
  protected:
    void fetch_match_data_();
    void parse_match_response_(const std::string &response);
    void update_match_state_();
    
    std::string format_team_name_(const std::string &logo_filename);
    std::string normalize_team_name_(const std::string &team_name);
    std::string add_spacing_(const std::string &text);
    std::string clip_team_name_(const std::string &team_name, int max_width_px, font::Font *font);
    void draw_text_with_spacing_(int x, int y, font::Font *font, Color color,
                   const std::string &text, int spacing_px,
                   display::TextAlign align = display::TextAlign::TOP_LEFT);
    image::Image* get_team_logo_(const std::string &team_name);
    
    void draw_scheduled_mode_();
    void draw_today_pending_mode_();
    void draw_in_progress_mode_();
    void draw_finished_mode_();
    
    void draw_team_row_(int y, const Team &team, bool is_favorite, image::Image *logo);
    void draw_date_time_(int x, int y, time_t match_time);
    void draw_countdown_(int x, int y, int hours, int minutes);
    void draw_score_(int x, int y, int home_score, int away_score);
    void draw_time_in_match_(int x, int y, int minutes, int seconds, bool pulse);
    
    display::Display *display_ = nullptr;
    font::Font *font_ = nullptr;
    font::Font *small_font_ = nullptr;
    time::RealTimeClock *rtc_ = nullptr;
    http_request::HttpRequestComponent *http_request_ = nullptr;
    
    std::string api_key_;
    std::string favorite_team_;
    int team_id_ = 0;
    bool test_mode_ = false;
    std::string test_server_url_;
    
    Match current_match_;
    bool has_match_data_ = false;
    bool initial_fetch_done_ = false;
    unsigned long last_fetch_ = 0;
    unsigned long last_update_ = 0;
    bool colon_visible_ = true;
    
    std::map<std::string, image::Image*> team_logos_;
    std::map<std::string, image::Image*> logo_cache_;  // Cache for team name -> logo lookups
    
    // Polling interval: 5 minutes normally, 1 second in test mode
    #ifdef SOCCER_TEST_MODE
    static constexpr unsigned long FETCH_INTERVAL = 1000; // 1 second (test)
    #else
    static constexpr unsigned long FETCH_INTERVAL = 300000; // 5 minutes
    #endif
    static constexpr unsigned long UPDATE_INTERVAL = 1000;   // 1 second
};

}  // namespace soccer_tracker
}  // namespace esphome
