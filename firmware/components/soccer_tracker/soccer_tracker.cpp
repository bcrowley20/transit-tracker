#include "soccer_tracker.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"
#include <ctime>
#include <algorithm>
#include <cctype>
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace soccer_tracker {

static const char *TAG = "soccer_tracker";

// Chunked-transfer decoding that handles multiple chunks
static bool dechunk_(const std::string &in, std::string &out) {
  size_t pos = 0;
  out.clear();

  // Check if this looks like chunked encoding
  size_t first_crlf = in.find("\r\n", 0);
  if (first_crlf == std::string::npos || first_crlf == 0) {
    // No chunk header or empty, treat as non-chunked
    out = in;
    return true;
  }

  // Check if first line is a hex number
  std::string first_line = in.substr(0, first_crlf);
  if (first_line.empty() || !isxdigit(first_line[0])) {
    // Not chunked encoding
    out = in;
    return true;
  }

  // Process all chunks
  while (pos < in.size()) {
    // Find chunk size line
    size_t line_end = in.find("\r\n", pos);
    if (line_end == std::string::npos) {
      // Incomplete - return what we have
      return out.size() > 0;
    }

    std::string size_str = in.substr(pos, line_end - pos);
    
    // Parse hex chunk size
    size_t chunk_size = 0;
    for (char c : size_str) {
      uint8_t v;
      if (c >= '0' && c <= '9') v = c - '0';
      else if (c >= 'a' && c <= 'f') v = 10 + (c - 'a');
      else if (c >= 'A' && c <= 'F') v = 10 + (c - 'A');
      else {
        // Invalid hex - if we have data, return it
        return out.size() > 0;
      }
      chunk_size = (chunk_size << 4) + v;
    }

    pos = line_end + 2;  // skip CRLF after chunk size

    if (chunk_size == 0) {
      // Final chunk (0\r\n) - we're done
      return true;
    }

    // Extract chunk data
    size_t available = in.size() - pos;
    if (available < chunk_size) {
      // Incomplete chunk - take what we can get
      if (available > 0) {
        out.append(in, pos, available);
      }
      return out.size() > 0;
    }

    // Full chunk available
    out.append(in, pos, chunk_size);
    pos += chunk_size;

    // Skip CRLF after chunk data
    if (pos + 2 <= in.size() && in[pos] == '\r' && in[pos + 1] == '\n') {
      pos += 2;
    } else {
      // Missing CRLF but we have data
      return out.size() > 0;
    }
  }

  return out.size() > 0;
}

// Helper function to parse ISO 8601 datetime
static bool parse_iso8601(const std::string &datetime_str, time_t &result) {
  struct tm tm_time = {};
  
  // Parse format: YYYY-MM-DDTHH:MM:SSZ
  if (sscanf(datetime_str.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d",
             &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday,
             &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec) != 6) {
    return false;
  }
  
  tm_time.tm_year -= 1900;  // Years since 1900
  tm_time.tm_mon -= 1;      // Months since January (0-11)
  tm_time.tm_isdst = 0;     // No DST for UTC
  
  result = mktime(&tm_time);
  return result != -1;
}

void SoccerTracker::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Soccer Tracker...");

  // Register a simple config endpoint on the embedded web server
  if (web_server_base::global_web_server_base != nullptr) {
    auto server = web_server_base::global_web_server_base->get_server();
    if (server != nullptr) {
      // GET /soccer/config?debug=0|1&url=http://ip:port
      class Handler : public AsyncWebHandler {
       public:
        explicit Handler(SoccerTracker *tracker) : tracker_(tracker) {}
        bool canHandle(AsyncWebServerRequest *request) const override {
          return request->url() == "/soccer/config" || request->url() == "/soccer";
        }
        void handleRequest(AsyncWebServerRequest *request) override {
          if (request->url() == "/soccer") {
            std::string html = "<html><head><title>Soccer Config</title></head><body>";
            html += "<h3>Soccer Tracker Config</h3>";
            html += "<form action=/soccer/config method=get>";
            html += "Debug Mode: <input type=checkbox name=debug value=1";
            if (tracker_->get_test_mode()) html += " checked";
            html += ">";
            html += "<br/>Test Server URL: <input type=text name=url value=\"" + tracker_->get_test_server_url() + "\" size=32>";
            html += "<br/><input type=submit value=Save></form>";
            html += "</body></html>";
            auto *res = request->beginResponse(200, "text/html", html);
            request->send(res);
            return;
          }
          // Parse params
          bool debug_on = tracker_->get_test_mode();
          if (request->hasParam("debug")) {
            auto dp = request->getParam("debug");
            if (dp != nullptr) {
              debug_on = dp->value().length() > 0;
            }
          }
          std::string url_val = tracker_->get_test_server_url();
          if (request->hasParam("url")) {
            auto up = request->getParam("url");
            if (up != nullptr) {
              url_val = up->value().c_str();
            }
          }
          tracker_->set_test_mode(debug_on);
          tracker_->set_test_server_url(url_val);
          auto *res_json = request->beginResponse(200, "application/json", std::string("{\"ok\":true}"));
          request->send(res_json);
        }
        SoccerTracker *tracker_;
      };
      server->addHandler(new Handler(this));  // NOLINT
    }
  }
  
  // Check immediately if RTC is already valid
  if (this->rtc_->now().is_valid()) {
    ESP_LOGI(TAG, "RTC already valid at setup, doing initial fetch now");
    this->fetch_match_data_();
    // initial_fetch_done_ will be set to true by fetch_match_data_ if successful
  } else {
    ESP_LOGW(TAG, "RTC not yet valid at setup, will retry");
  }
  
  // Check for RTC validity every 2 seconds and do initial fetch when ready
  this->set_interval("rtc_check", 2000, [this]() {
    bool rtc_valid = this->rtc_->now().is_valid();
    if (!this->initial_fetch_done_ && rtc_valid) {
      ESP_LOGI(TAG, "RTC now valid, starting initial fetch");
      this->fetch_match_data_();
      // initial_fetch_done_ will be set to true by fetch_match_data_ if successful
    }
  });
  
  // Set up periodic fetching (will only run after initial fetch succeeds)
  this->set_interval("fetch_matches", FETCH_INTERVAL, [this]() {
    ESP_LOGD(TAG, "Periodic fetch check: initial_fetch_done=%d", this->initial_fetch_done_);
    if (this->initial_fetch_done_) {
      // In test mode, also enforce a faster polling interval
      if (this->test_mode_) {
        if (millis() - this->last_fetch_ >= 10000) {
          this->fetch_match_data_();
        }
      } else {
        this->fetch_match_data_();
      }
    }
  });
}

void SoccerTracker::loop() {
  // Update display every second
  if (millis() - this->last_update_ >= UPDATE_INTERVAL) {
    this->last_update_ = millis();
    
    if (this->has_match_data_) {
      this->update_match_state_();
      this->colon_visible_ = !this->colon_visible_; // Toggle for heartbeat effect
    }
  }
}

void SoccerTracker::dump_config() {
  ESP_LOGCONFIG(TAG, "Soccer Tracker:");
  ESP_LOGCONFIG(TAG, "  Favorite Team: %s", this->favorite_team_.c_str());
  ESP_LOGCONFIG(TAG, "  Team ID: %d", this->team_id_);
  ESP_LOGCONFIG(TAG, "  Registered Logos: %d", this->team_logos_.size());
}

void SoccerTracker::fetch_match_data_() {
  if (!network::is_connected()) {
    ESP_LOGW(TAG, "Not connected to network, skipping fetch");
    return;
  }
  
  if (!this->rtc_->now().is_valid()) {
    ESP_LOGW(TAG, "RTC time not valid, skipping fetch");
    return;
  }
  
  if (this->api_key_.empty() || this->team_id_ == 0) {
    ESP_LOGW(TAG, "API key or team ID not configured");
    return;
  }
  
  if (this->http_request_ == nullptr) {
    ESP_LOGE(TAG, "HTTP request component not initialized!");
    return;
  }
  
  ESP_LOGD(TAG, "Fetching match data for team %d", this->team_id_);
  
  // Build the API URL for team fixtures. Allow override via local test server when in test mode.
  char url[256];
  if (this->test_mode_ && !this->test_server_url_.empty()) {
    std::string base = this->test_server_url_;
    if (base.rfind("http://", 0) != 0 && base.rfind("https://", 0) != 0) {
      base = std::string("http://") + base;
    }
    snprintf(url, sizeof(url), "%s/fixtures", base.c_str());
  } else {
    // API-Football: next=1 for single upcoming fixture
    snprintf(url, sizeof(url), "https://v3.football.api-sports.io/fixtures?team=%d&next=1", 
             this->team_id_);
  }
  
  ESP_LOGD(TAG, "API URL: %s", url);
  
  // Prepare headers for API-Football
  std::list<http_request::Header> headers;
  headers.push_back(http_request::Header{"x-apisports-key", this->api_key_});
  // Force plain (non-gzip) response so ArduinoJson can parse without a decompressor
  headers.push_back(http_request::Header{"Accept-Encoding", "identity"});
  
  // Make synchronous HTTP GET request (ESPHome's http_request doesn't support async callbacks)
  ESP_LOGD(TAG, "Making HTTP GET request...");
  auto response = this->http_request_->get(url, headers);
  ESP_LOGD(TAG, "HTTP request returned");
  
  if (response == nullptr) {
    ESP_LOGW(TAG, "HTTP request returned null response");
    this->last_fetch_ = millis();
    return;
  }
  
  ESP_LOGD(TAG, "HTTP response status: %d, content_length: %zu", response->status_code, response->content_length);
  
  if (response->status_code != 200) {
    ESP_LOGW(TAG, "HTTP request failed with code: %d", response->status_code);
    this->last_fetch_ = millis();
    response->end();
    return;
  }
  
  // Read response body; stop cleanly at Content-Length to avoid extra reads
  std::string response_str;
  size_t max_length = 8192;  // 8KB safety cap
  response_str.reserve(max_length);

  uint8_t buf[256];
  unsigned long read_timeout = millis() + 2000;  // 2s timeout
  int consecutive_short_reads = 0;
  int idle_reads = 0;
  const bool length_known = (response->content_length > 0 && response->content_length < max_length);

  // Small delay to ensure response body is ready
  delay(20);

  while (response_str.size() < max_length && millis() < read_timeout) {
    // If length is known and we've read enough, stop
    if (length_known && response_str.size() >= response->content_length) {
      break;
    }

    int read_len = response->read(buf, sizeof(buf));
    ESP_LOGD(TAG, "Read attempt: %d bytes", read_len);

    if (read_len > 0) {
      response_str.append((char *)buf, read_len);
      read_timeout = millis() + 2000;  // reset timeout on progress
      idle_reads = 0;

      if (!length_known) {
        // Heuristic EOF for unknown-length responses
        if (read_len < (int)sizeof(buf)) {
          consecutive_short_reads++;
          if (consecutive_short_reads >= 3) break;
        } else {
          consecutive_short_reads = 0;
        }
      }
    } else {
      // No data; avoid hammering the stream
      idle_reads++;
      if (idle_reads >= 3) break;  // stream likely closed
      yield();
    }
  }
  ESP_LOGD(TAG, "Read %zu bytes from response", response_str.length());
  
  response->end();
  
  if (response_str.empty()) {
    ESP_LOGW(TAG, "Empty response received");
    this->last_fetch_ = millis();
    return;
  }
  
  ESP_LOGD(TAG, "Received response (%zu bytes)", response_str.length());

  // Debug: log the first part of the payload to diagnose parse errors
  const size_t preview_len = std::min<size_t>(response_str.size(), 200);
  std::string preview = response_str.substr(0, preview_len);
  // Also log first few bytes in hex to detect gzip/binary responses
  char hexbuf[200];
  size_t hex_len = std::min<size_t>(response_str.size(), 32);
  size_t pos_hex = 0;
  for (size_t i = 0; i < hex_len && pos_hex + 3 < sizeof(hexbuf); i++) {
    pos_hex += snprintf(hexbuf + pos_hex, sizeof(hexbuf) - pos_hex, "%02X ", (uint8_t)response_str[i]);
  }
  hexbuf[std::min<size_t>(pos_hex, sizeof(hexbuf) - 1)] = '\0';
  ESP_LOGD(TAG, "Response first bytes (hex): %s", hexbuf);

  // Also log last bytes to see if we got the terminating chunk
  if (response_str.size() > 32) {
    char hexbuf_end[200];
    size_t hex_end = std::min<size_t>(response_str.size() - 32, 32);
    size_t pos_hex_end = 0;
    for (size_t i = response_str.size() - hex_end; i < response_str.size() && pos_hex_end + 3 < sizeof(hexbuf_end); i++) {
      pos_hex_end += snprintf(hexbuf_end + pos_hex_end, sizeof(hexbuf_end) - pos_hex_end, "%02X ", (uint8_t)response_str[i]);
    }
    hexbuf_end[std::min<size_t>(pos_hex_end, sizeof(hexbuf_end) - 1)] = '\0';
    ESP_LOGD(TAG, "Response last bytes (hex): %s", hexbuf_end);
  }

  // Handle chunked transfer encoding (Content-Length: -1 came through as UINT_MAX)
  if (!response_str.empty() && isxdigit(response_str[0])) {
    std::string dechunked;
    if (dechunk_(response_str, dechunked)) {
      ESP_LOGD(TAG, "Dechunked body size: %zu", dechunked.size());
      response_str.swap(dechunked);
    }
  }
  
  this->parse_match_response_(response_str);
  this->last_fetch_ = millis();
  
  // Mark initial fetch as done only after successful parse
  if (this->has_match_data_) {
    this->initial_fetch_done_ = true;
    ESP_LOGI(TAG, "Initial fetch successful, match data available");
  }
}

void SoccerTracker::parse_match_response_(const std::string &response) {
  bool parsed = json::parse_json(response, [this](JsonObject root) -> bool {
    // API-Football response structure: { "get": "fixtures", "results": N, "response": [...] }
    if (!root.containsKey("response")) {
      ESP_LOGW(TAG, "Response does not contain 'response' key");
      return false;
    }
    
    JsonArray fixtures = root["response"].as<JsonArray>();
    if (fixtures.size() == 0) {
      ESP_LOGW(TAG, "No fixtures found");
      return false;
    }
    
    // Get first fixture (we only requested 1)
    JsonObject next_match = fixtures[0];
    
    // Validate fixture structure
    if (!next_match.containsKey("fixture") || !next_match.containsKey("teams") || !next_match.containsKey("goals")) {
      ESP_LOGW(TAG, "Fixture missing required fields");
      return false;
    }
    
    // Parse fixture info
    JsonObject fixture_info = next_match["fixture"];
    if (!fixture_info.containsKey("date") || !fixture_info.containsKey("status")) {
      ESP_LOGW(TAG, "Fixture info missing date or status");
      return false;
    }
    
    std::string match_date_str = fixture_info["date"].as<std::string>();
    
    if (!parse_iso8601(match_date_str, this->current_match_.match_time)) {
      ESP_LOGW(TAG, "Failed to parse match date: %s", match_date_str.c_str());
      return false;
    }
    
    // Parse home team
    JsonObject teams = next_match["teams"];
    if (!teams.containsKey("home") || !teams.containsKey("away")) {
      ESP_LOGW(TAG, "Teams missing home or away");
      return false;
    }
    
    JsonObject home_team_obj = teams["home"];
    if (!home_team_obj.containsKey("name")) {
      ESP_LOGW(TAG, "Home team missing name");
      return false;
    }
    this->current_match_.home_team.name = home_team_obj["name"].as<std::string>();
    this->current_match_.home_team.score = home_team_obj["goals"].as<int>();
    
    // Parse away team
    JsonObject away_team_obj = teams["away"];
    if (!away_team_obj.containsKey("name")) {
      ESP_LOGW(TAG, "Away team missing name");
      return false;
    }
    this->current_match_.away_team.name = away_team_obj["name"].as<std::string>();
    this->current_match_.away_team.score = away_team_obj["goals"].as<int>();
    
    // Get status
    std::string status = fixture_info["status"].as<std::string>();
    time_t now_time = this->rtc_->now().timestamp;
    
    // Determine match state based on status codes
    if (status == "1H" || status == "2H" || status == "HT" || status == "ET" || status == "BT" || status == "P" || status == "LIVE") {
      this->current_match_.state = IN_PROGRESS;
      this->current_match_.minute = 0;
      this->current_match_.second = 0;
      
    } else if (status == "FT" || status == "AET" || status == "PEN") {
      this->current_match_.state = FINISHED;
      this->current_match_.finish_time = now_time;
      
    } else if (status == "NS" || status == "TBD") {
      // Not Started / Scheduled - use local timezone for date comparison
      ESPTime now_local = this->rtc_->now();
      ESPTime match_local = ESPTime::from_epoch_local(this->current_match_.match_time);
      
      bool is_today = (now_local.year == match_local.year &&
               now_local.month == match_local.month &&
               now_local.day_of_month == match_local.day_of_month);
      
      // Use local timestamps for comparison to avoid UTC skew
      if (is_today && match_local.timestamp > now_local.timestamp) {
        this->current_match_.state = TODAY_PENDING;
      } else {
        this->current_match_.state = SCHEDULED;
      }
    } else {
      this->current_match_.state = SCHEDULED;
    }
    
    this->has_match_data_ = true;

    const char *state_str = "SCHEDULED";
    switch (this->current_match_.state) {
      case SCHEDULED: state_str = "SCHEDULED"; break;
      case TODAY_PENDING: state_str = "TODAY_PENDING"; break;
      case IN_PROGRESS: state_str = "IN_PROGRESS"; break;
      case FINISHED: state_str = "FINISHED"; break;
    }

    ESP_LOGD(TAG, "Match classified: %s vs %s -> %s (status: %s)",
             this->current_match_.home_team.name.c_str(),
             this->current_match_.away_team.name.c_str(),
             state_str,
             status.c_str());
    
    return true;
  });
  
  if (!parsed) {
    ESP_LOGW(TAG, "Failed to parse match response");
  }
}

void SoccerTracker::update_match_state_() {
  if (!this->has_match_data_) return;
  
  time_t now = this->rtc_->now().timestamp;
  
  // Check if FINISHED state should expire (after 1 hour)
  if (this->current_match_.state == FINISHED) {
    if (now - this->current_match_.finish_time > 3600) {
      // Fetch new match data
      this->fetch_match_data_();
      return;
    }
  }
  
  // Update IN_PROGRESS timing
  if (this->current_match_.state == IN_PROGRESS) {
    int elapsed = now - this->current_match_.match_time;
    this->current_match_.minute = elapsed / 60;
    this->current_match_.second = elapsed % 60;
    
    // Cap at 90 minutes (could be extended for extra time)
    if (this->current_match_.minute > 90) {
      this->current_match_.minute = 90;
    }
  }
  
  // Check if SCHEDULED becomes TODAY_PENDING (use local timezone)
  if (this->current_match_.state == SCHEDULED) {
    ESPTime now_local = this->rtc_->now();
    ESPTime match_local = ESPTime::from_epoch_local(this->current_match_.match_time);
    
    bool is_today = (now_local.year == match_local.year &&
                     now_local.month == match_local.month &&
                     now_local.day_of_month == match_local.day_of_month);
    
    if (is_today && match_local.timestamp > now_local.timestamp) {
      this->current_match_.state = TODAY_PENDING;
    }
  }
  
  // Check if TODAY_PENDING should become IN_PROGRESS
  if (this->current_match_.state == TODAY_PENDING) {
    if (now >= this->current_match_.match_time) {
      // Fetch fresh data as match should be starting
      this->fetch_match_data_();
    }
  }
}

std::string SoccerTracker::format_team_name_(const std::string &logo_filename) {
  // Extract team name from filename like "atlanta-united-footballlogos-org_14x14.png"
  size_t pos = logo_filename.find("-footballlogos-org");
  if (pos == std::string::npos) return "";
  
  std::string team_name = logo_filename.substr(0, pos);
  
  // Replace hyphens with spaces
  for (char &c : team_name) {
    if (c == '-') c = ' ';
  }
  
  // Capitalize each word
  bool capitalize_next = true;
  for (char &c : team_name) {
    if (std::isspace(c)) {
      capitalize_next = true;
    } else if (capitalize_next) {
      c = std::toupper(c);
      capitalize_next = false;
    }
  }
  
  // Special case: "fc" should be "FC"
  size_t fc_pos = team_name.find(" fc");
  if (fc_pos != std::string::npos) {
    team_name[fc_pos + 1] = 'F';
    team_name[fc_pos + 2] = 'C';
  }
  fc_pos = team_name.find(" Fc");
  if (fc_pos != std::string::npos) {
    team_name[fc_pos + 2] = 'C';
  }
  
  return team_name;
}

std::string SoccerTracker::normalize_team_name_(const std::string &team_name) {
  std::string normalized = team_name;
  
  // Convert to lowercase and replace spaces with hyphens
  for (char &c : normalized) {
    if (std::isspace(c)) {
      c = '-';
    } else {
      c = std::tolower(c);
    }
  }
  
  // Remove common suffixes that might not be in logo names
  const std::string suffixes[] = {"-sc", "-united", "-fc", "-city"};
  // This is a simplistic match - in practice you'd want fuzzy matching
  
  return normalized;
}

// Insert a single space between non-space characters to improve legibility
std::string SoccerTracker::add_spacing_(const std::string &text) {
  std::string spaced;
  spaced.reserve(text.size() * 2);

  for (size_t i = 0; i < text.size(); i++) {
    spaced.push_back(text[i]);
    // Add a space if the current and next characters are not spaces
    if (i + 1 < text.size() && text[i] != ' ' && text[i + 1] != ' ') {
      spaced.push_back(' ');
    }
  }

  return spaced;
}

// Draw text with custom per-character spacing and alignment (supports TOP_LEFT and TOP_RIGHT)
void SoccerTracker::draw_text_with_spacing_(int x, int y, font::Font *font, Color color,
                                           const std::string &text, int spacing_px,
                                           display::TextAlign align) {
  if (font == nullptr || this->display_ == nullptr) return;

  // Compute total width
  int total_width = 0;
  for (size_t i = 0; i < text.size(); i++) {
    char c_str[2] = {text[i], '\0'};
    int w = 0, xo = 0, bl = 0, h = 0;
    font->measure(c_str, &w, &xo, &bl, &h);
    total_width += w;
    if (i + 1 < text.size()) total_width += spacing_px;
  }

  int start_x = x;
  if (align == display::TextAlign::TOP_RIGHT) {
    start_x = x - total_width;
  }

  int cursor_x = start_x;
  for (size_t i = 0; i < text.size(); i++) {
    char c_str[2] = {text[i], '\0'};
    this->display_->print(cursor_x, y, font, color, display::TextAlign::TOP_LEFT, c_str);
    int w = 0, xo = 0, bl = 0, h = 0;
    font->measure(c_str, &w, &xo, &bl, &h);
    cursor_x += w + spacing_px;
  }
}

image::Image* SoccerTracker::get_team_logo_(const std::string &team_name) {
  // Check cache first
  auto cache_it = this->logo_cache_.find(team_name);
  if (cache_it != this->logo_cache_.end()) {
    return cache_it->second;
  }
  
  // Cache miss - perform lookup
  std::string normalized = this->normalize_team_name_(team_name);
  ESP_LOGD(TAG, "Cache miss - looking for logo for team: '%s' (normalized: '%s')", team_name.c_str(), normalized.c_str());
  
  // Search through registered logos
  for (auto &pair : this->team_logos_) {
    std::string logo_team = this->format_team_name_(pair.first);
    std::string logo_normalized = this->normalize_team_name_(logo_team);
    
    ESP_LOGD(TAG, "  Checking logo: '%s' -> '%s' (normalized: '%s')", 
             pair.first.c_str(), logo_team.c_str(), logo_normalized.c_str());
    
    // Check if names match
    if (normalized.find(logo_normalized) != std::string::npos ||
        logo_normalized.find(normalized) != std::string::npos) {
      ESP_LOGD(TAG, "  MATCH! Using logo: %s for team: %s", pair.first.c_str(), team_name.c_str());
      // Cache the result
      this->logo_cache_[team_name] = pair.second;
      return pair.second;
    }
  }
  
  ESP_LOGW(TAG, "No logo found for team: %s", team_name.c_str());
  return nullptr;
}

void SoccerTracker::draw_match() {
  if (this->display_ == nullptr) {
    ESP_LOGW(TAG, "No display attached");
    return;
  }
  
  if (!this->has_match_data_) {
    // Draw loading indicator with timestamp
    auto now_time = this->rtc_->now();
    unsigned long time_since_fetch = millis() - this->last_fetch_;
    
    int x = this->display_->get_width() / 2;
    int y = this->display_->get_height() / 2 - 8;
    
    this->display_->printf(x, y, this->font_, Color(255, 255, 255), 
                          display::TextAlign::CENTER, "Loading...");
    
    // Show time since last fetch attempt for debugging
    char status[32];
    snprintf(status, sizeof(status), "%lu ms", time_since_fetch);
    this->display_->printf(x, y + 10, this->font_, Color(0, 230, 0), 
                          display::TextAlign::CENTER, status);
    return;
  }
  
  switch (this->current_match_.state) {

    case SCHEDULED:
      this->draw_scheduled_mode_();
      break;
    case TODAY_PENDING:
      this->draw_today_pending_mode_();
      break;
    case IN_PROGRESS:
      this->draw_in_progress_mode_();
      break;
    case FINISHED:
      break;
  }
}

std::string SoccerTracker::clip_team_name_(const std::string &team_name, int max_width_px, font::Font *font) {
  if (font == nullptr) return team_name;
  
  int current_width = 0;
  std::string result;
  
  for (size_t i = 0; i < team_name.size(); i++) {
    char c_str[2] = {team_name[i], '\0'};
    int w = 0, xo = 0, bl = 0, h = 0;
    font->measure(c_str, &w, &xo, &bl, &h);
    
    if (current_width + w > max_width_px) {
      // Add ellipsis if we can fit it
      if (current_width + 6 <= max_width_px) {
        result += "...";
      }
      break;
    }
    result += team_name[i];
    current_width += w;
  }
  
  return result;
}

void SoccerTracker::draw_team_row_(int y, const Team &team, bool is_favorite, image::Image *logo) {
  int x = 0;
  int text_y = y;
  
  // Draw logo if available
  if (logo != nullptr) {
    this->display_->image(x, y, logo);
    x += logo->get_width() + 2; // 2 pixel gap
    // Center text vertically with logo (logo is 14px, font ~8px, offset by 3px)
    text_y = y + 3;
  }
  
  // Clip team name to avoid overlapping date/time (right-side area starts ~60px from right edge)
  int max_name_width = this->display_->get_width() - x - 35;
  std::string clipped_name = this->clip_team_name_(team.name, max_name_width, this->font_);
  
  // Draw team name
  this->display_->print(x, text_y, this->font_, Color(255, 255, 255), clipped_name.c_str());
}

void SoccerTracker::draw_date_time_(int x, int y, time_t match_time) {
  struct tm *tm_time = localtime(&match_time);
  
  char date_str[16];
  char time_str[16];
  
  // Format: MM-DD-YY
  strftime(date_str, sizeof(date_str), "%m-%d-%y", tm_time);
  // Format: HH:MM (24-hour)
  strftime(time_str, sizeof(time_str), "%H:%M", tm_time);
  
  this->display_->printf(x, y, this->small_font_, Color(255, 255, 255), 
                        display::TextAlign::TOP_RIGHT, "%s", date_str);
  this->display_->printf(x, y + 8, this->small_font_, Color(255, 255, 255),
                        display::TextAlign::TOP_RIGHT, "%s", time_str);
}

void SoccerTracker::draw_countdown_(int x, int y, int hours, int minutes) {
  char countdown_str[16];
  snprintf(countdown_str, sizeof(countdown_str), "%02d%c%02d", 
           hours, (this->colon_visible_ ? ':' : ' '), minutes);
  
  this->display_->printf(x, y, this->font_, Color(255, 255, 0),
                        display::TextAlign::TOP_RIGHT, "%s", countdown_str);
}

void SoccerTracker::draw_score_(int x, int y, int home_score, int away_score) {
  this->display_->printf(x, y, this->font_, Color(255, 255, 255),
                        display::TextAlign::TOP_RIGHT, "%d", home_score);
  this->display_->printf(x, y + 16, this->font_, Color(255, 255, 255),
                        display::TextAlign::TOP_RIGHT, "%d", away_score);
}

void SoccerTracker::draw_time_in_match_(int x, int y, int minutes, int seconds, bool pulse) {
  char time_str[16];
  snprintf(time_str, sizeof(time_str), "%02d%c%02d", 
           minutes, (pulse && this->colon_visible_ ? ':' : ' '), seconds);
  
  this->display_->printf(x, y, this->small_font_, Color(255, 255, 255),
                        display::TextAlign::TOP_RIGHT, "%s", time_str);
}

void SoccerTracker::draw_scheduled_mode_() {
  // Determine which team is favorite
  bool home_is_favorite = (this->current_match_.home_team.name == this->favorite_team_);
  
  // Home team on bottom
  this->draw_team_row_(0, this->current_match_.away_team, false, this->get_team_logo_(this->current_match_.away_team.name));
  this->draw_team_row_(16, this->current_match_.home_team, false, this->get_team_logo_(this->current_match_.home_team.name));
  
  // Convert match time to local timezone using ESPHome RTC
  ESPTime match_time = ESPTime::from_epoch_local(this->current_match_.match_time);
  
  char date_str[16];
  char time_str[16];
  
  // Format date: MM-DD-YY
  snprintf(date_str, sizeof(date_str), "%02d-%02d-%02d",
           match_time.month, match_time.day_of_month, match_time.year % 100);
  
  // Format time: HH:MM am/pm (12-hour format)
  int hour_12 = match_time.hour % 12;
  if (hour_12 == 0) hour_12 = 12;
  const char* am_pm = (match_time.hour < 12) ? "am" : "pm";
  snprintf(time_str, sizeof(time_str), "%02d:%02d %s",
           hour_12, match_time.minute, am_pm);
  
  // Draw date and time centered vertically on the right side in green, using larger font for legibility
  int right_x = this->display_->get_width();
  int center_y = this->display_->get_height() / 2;
  int date_y = center_y - 10;  // font_ is taller than small_font_
  int time_y = center_y + 2;   // small gap between lines

  Color green(0, 255, 0);
  // Render with minimal extra spacing (0px) since the font has built-in ~1px advance
  this->draw_text_with_spacing_(right_x, date_y, this->font_, green, date_str, 0, display::TextAlign::TOP_RIGHT);
  this->draw_text_with_spacing_(right_x, time_y, this->font_, green, time_str, 0, display::TextAlign::TOP_RIGHT);
}

void SoccerTracker::draw_today_pending_mode_() {
  // Similar to scheduled but show countdown instead of date
  bool home_is_favorite = (this->current_match_.home_team.name == this->favorite_team_);
  
  if (home_is_favorite) {
    this->draw_team_row_(0, this->current_match_.home_team, true, this->get_team_logo_(this->current_match_.home_team.name));
    this->draw_team_row_(16, this->current_match_.away_team, false, this->get_team_logo_(this->current_match_.away_team.name));
  } else {
    this->draw_team_row_(0, this->current_match_.away_team, false, this->get_team_logo_(this->current_match_.away_team.name));
    this->draw_team_row_(16, this->current_match_.home_team, false, this->get_team_logo_(this->current_match_.home_team.name));
  }
  
  // Calculate time until match
  time_t now = this->rtc_->now().timestamp;
  int seconds_until = this->current_match_.match_time - now;
  int hours = seconds_until / 3600;
  int minutes = (seconds_until % 3600) / 60;
  
  // Draw countdown with pulsing colon
  this->draw_countdown_(this->display_->get_width(), 8, hours, minutes);
}

void SoccerTracker::draw_in_progress_mode_() {
  // Draw teams with current score
  bool home_is_favorite = (this->current_match_.home_team.name == this->favorite_team_);
  
  if (home_is_favorite) {
    this->draw_team_row_(0, this->current_match_.home_team, true, this->get_team_logo_(this->current_match_.home_team.name));
    this->draw_team_row_(16, this->current_match_.away_team, false, this->get_team_logo_(this->current_match_.away_team.name));
  } else {
    this->draw_team_row_(0, this->current_match_.away_team, false, this->get_team_logo_(this->current_match_.away_team.name));
    this->draw_team_row_(16, this->current_match_.home_team, false, this->get_team_logo_(this->current_match_.home_team.name));
  }
  
  // Draw score on right side
  int score_x = this->display_->get_width() - 20;
  if (home_is_favorite) {
    this->draw_score_(this->display_->get_width(), 0, 
                     this->current_match_.home_team.score,
                     this->current_match_.away_team.score);
  } else {
    this->draw_score_(this->display_->get_width(), 0,
                     this->current_match_.away_team.score,
                     this->current_match_.home_team.score);
  }
  
  // Draw match time (MM:SS)
  this->draw_time_in_match_(score_x - 25, 0, 
                           this->current_match_.minute,
                           this->current_match_.second, true);
}

void SoccerTracker::draw_finished_mode_() {
  // Similar to in_progress but with "F" instead of time
  bool home_is_favorite = (this->current_match_.home_team.name == this->favorite_team_);
  
  if (home_is_favorite) {
    this->draw_team_row_(0, this->current_match_.home_team, true, this->get_team_logo_(this->current_match_.home_team.name));
    this->draw_team_row_(16, this->current_match_.away_team, false, this->get_team_logo_(this->current_match_.away_team.name));
  } else {
    this->draw_team_row_(0, this->current_match_.away_team, false, this->get_team_logo_(this->current_match_.away_team.name));
    this->draw_team_row_(16, this->current_match_.home_team, false, this->get_team_logo_(this->current_match_.home_team.name));
  }
  
  // Draw final score
  if (home_is_favorite) {
    this->draw_score_(this->display_->get_width(), 0,
                     this->current_match_.home_team.score,
                     this->current_match_.away_team.score);
  } else {
    this->draw_score_(this->display_->get_width(), 0,
                     this->current_match_.away_team.score,
                     this->current_match_.home_team.score);
  }
  
  // Draw "F" for final
  this->display_->printf(this->display_->get_width() - 25, 12, this->font_, 
                        Color(255, 0, 0), display::TextAlign::TOP_RIGHT, "F");
}

}  // namespace soccer_tracker
}  // namespace esphome
