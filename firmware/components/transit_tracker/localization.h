#pragma once

#include <string>

#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace transit_tracker {

enum UnitDisplay : uint8_t {
  UNIT_DISPLAY_LONG,
  UNIT_DISPLAY_SHORT,
  UNIT_DISPLAY_NONE
};

class Localization {
  public:
    // Inline implementation to ensure availability during linking
    std::string fmt_duration_from_now(time_t unix_timestamp, uint rtc_now) const {
      long diff = static_cast<long>(unix_timestamp - rtc_now);

      if (diff < 30) {
        return this->now_string_;
      }

      if (diff < 60) {
        switch (this->unit_display_) {
          case UNIT_DISPLAY_LONG:
            return std::string("0") + this->minutes_long_string_;
          case UNIT_DISPLAY_SHORT:
            return std::string("0") + this->minutes_short_string_;
          case UNIT_DISPLAY_NONE:
            return std::string("0");
        }
      }

      int minutes = static_cast<int>(diff / 60);

      if (minutes < 60) {
        switch (this->unit_display_) {
          case UNIT_DISPLAY_LONG:
            return std::to_string(minutes) + this->minutes_long_string_;
          case UNIT_DISPLAY_SHORT:
            return std::to_string(minutes) + this->minutes_short_string_;
          case UNIT_DISPLAY_NONE:
          default:
            return std::to_string(minutes);
        }
      }

      int hours = minutes / 60;
      minutes = minutes % 60;

      switch (this->unit_display_) {
        case UNIT_DISPLAY_LONG:
        case UNIT_DISPLAY_SHORT: {
          std::string out = std::to_string(hours) + this->hours_short_string_ + std::to_string(minutes) + this->minutes_short_string_;
          return out;
        }
        case UNIT_DISPLAY_NONE:
        default: {
          char buf[16];
          snprintf(buf, sizeof(buf), "%d:%02d", hours, minutes);
          return std::string(buf);
        }
      }
    }

    void set_unit_display(UnitDisplay unit_display) { unit_display_ = unit_display; }
    void set_now_string(const std::string &now_string) { now_string_ = now_string; }
    void set_minutes_long_string(const std::string &minutes_long_string) { minutes_long_string_ = minutes_long_string; }
    void set_minutes_short_string(const std::string &minutes_short_string) { minutes_short_string_ = minutes_short_string; }
    void set_hours_short_string(const std::string &hours_short_string) { hours_short_string_ = hours_short_string; }

  protected:
    UnitDisplay unit_display_ = UNIT_DISPLAY_LONG;
    std::string now_string_ = "Now";
    std::string minutes_long_string_ = "min";
    std::string minutes_short_string_ = "m";
    std::string hours_short_string_ = "h";
};

}  // namespace transit_tracker
}  // namespace esphome
