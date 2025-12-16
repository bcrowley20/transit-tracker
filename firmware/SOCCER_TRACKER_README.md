# Soccer Tracker for Matrix Portal S3

A soccer/football match tracker display for the Matrix Portal S3 ESP32 board running ESPHome. This component displays live match information for your favorite soccer team using data from the [football-data.org API](https://www.football-data.org/).

## Features

The display has **four distinct modes** that automatically transition based on match status:

### Mode 1: Scheduled Match (Not Today)
- Shows favorite team name and logo on top row
- Shows opponent team name and logo on second row
- Displays match date (MM-DD-YY) and time (HH:MM 24-hour format) on the right

### Mode 2: Match Day Countdown (Match Today, Not Started)
- Same layout as Mode 1
- Instead of date, shows countdown to match start in HH:MM format
- Colon pulses once per second as a "heartbeat"

### Mode 3: Live Match (In Progress)
- Shows both teams with current score
- Displays match time in MM:SS format
- Score updates in real-time
- Colon pulses to indicate live updates

### Mode 4: Match Finished
- Shows final score
- Displays "F" (Final) indicator
- Remains visible for 1 hour after match completion
- Automatically returns to Mode 1 after 1 hour

## Hardware Requirements

- **Matrix Portal S3** (ESP32-S3 based)
- **64x32 or 64x64 HUB75 RGB LED Matrix Display** (chained for 128x32)
- **5V Power Supply** (adequate for your display size)
- **USB-C Cable** for programming

## Software Requirements

- **ESPHome** (2025.7.0 or later)
- **football-data.org API Key** (free tier available)

## Installation

### 1. Get Your API Key

1. Register for a free account at [football-data.org](https://www.football-data.org/client/register)
2. Copy your API key from the dashboard

### 2. Find Your Team ID

You can find team IDs by visiting:
```
https://api.football-data.org/v4/teams
```

Common MLS Team IDs (examples):
- Seattle Sounders FC: 756
- LA Galaxy: 3017
- Atlanta United FC: 73

### 3. Configure Secrets

1. Copy `secrets.yaml.template` to `secrets.yaml`
2. Fill in your WiFi credentials and API key:

```yaml
wifi_ssid: "YourWiFiNetwork"
wifi_password: "YourWiFiPassword"
football_data_api_key: "your_api_key_here"
```

### 4. Customize Team Configuration

Edit `soccer-tracker.yaml` and update these lines:

```yaml
soccer_tracker:
  favorite_team: "Seattle Sounders FC"  # Change to your team's full name
  team_id: 756  # Change to your team's ID from football-data.org
```

### 5. Flash to Device

```bash
esphome run soccer-tracker.yaml
```

## Configuration Options

### Main Configuration

| Option | Type | Required | Description |
|--------|------|----------|-------------|
| `api_key` | string | Yes | Your football-data.org API key |
| `favorite_team` | string | Yes | Full name of your favorite team |
| `team_id` | integer | Yes | Team ID from football-data.org API |
| `display_id` | ID | Yes | Reference to display component |
| `font_id` | ID | Yes | Primary font (8px recommended) |
| `small_font_id` | ID | Yes | Small font for time/date (6px recommended) |
| `time_id` | ID | Yes | Reference to time component |
| `http_request_id` | ID | Yes | Reference to HTTP request component |
| `team_logos` | map | Optional | Map of team logo filenames to image IDs |

### Display Layout

The 128x32 display is divided into:
- **Left side (~80px)**: Team logos (14x14px) + names
- **Right side (~48px)**: Date/time/score/countdown
- **Two rows**: One for favorite team, one for opponent

## Team Logo Matching

The component automatically matches team names from the API with logo files based on these rules:

1. Logo filenames follow the pattern: `{team-name}_footballlogos-org_14x14.png`
2. Team names are normalized (spaces → hyphens, lowercase)
3. Special handling for "FC" (always capitalized in display)
4. Fuzzy matching allows for slight name variations

### Supported Teams

The included configuration supports all MLS teams with pre-loaded logos. For other leagues, you'll need to:

1. Add team logo PNG files (14x14 pixels) to `logos/teams_resized/`
2. Add image entries in the YAML configuration
3. Register logos in the `team_logos` map

## API Rate Limits

The free tier of football-data.org allows:
- **10 requests per minute**
- **Limited number of competitions**

This component:
- Fetches match data every **5 minutes**
- Updates display every **1 second** (no API calls)
- Only fetches next/current match for your team

## Customization

### Change Update Frequency

Edit `soccer_tracker.cpp`:

```cpp
static constexpr unsigned long FETCH_INTERVAL = 300000; // 5 minutes (in milliseconds)
static constexpr unsigned long UPDATE_INTERVAL = 1000;   // 1 second
```

### Adjust Display Layout

Modify the drawing methods in `soccer_tracker.cpp`:
- `draw_team_row_()` - Team name and logo positioning
- `draw_date_time_()` - Date/time formatting and position
- `draw_score_()` - Score display layout
- Layout constants like `max_name_width`

### Change Fonts

Update font references in `soccer-tracker.yaml`:

```yaml
font:
  - file: "path/to/your/font.ttf"
    id: your_font
    size: 8
```

## Troubleshooting

### Display Shows "Loading..."
- Check network connection
- Verify API key is correct
- Check logs for HTTP errors: `esphome logs soccer-tracker.yaml`

### Team Logo Not Showing
- Verify logo file exists in `logos/teams_resized/`
- Check filename matches pattern: `{team-name}_footballlogos-org_14x14.png`
- Ensure logo is registered in `team_logos` map
- Review team name normalization in logs

### Match Not Updating
- API updates may be delayed (especially for lower-tier leagues)
- Free tier has limited competition coverage
- Check if your league/competition is supported by football-data.org

### Time Zone Issues
- Update timezone in `soccer-tracker.yaml`:
  ```yaml
  time:
    - platform: sntp
      timezone: America/Los_Angeles  # Change to your timezone
  ```

## File Structure

```
firmware/
├── soccer-tracker.yaml          # Main configuration file
├── secrets.yaml.template        # Template for secrets
├── secrets.yaml                 # Your secrets (not in git)
├── components/
│   └── soccer_tracker/
│       ├── __init__.py         # ESPHome component registration
│       ├── soccer_tracker.h    # C++ header
│       └── soccer_tracker.cpp  # C++ implementation
└── logos/
    └── teams_resized/
        └── *.png               # Team logo files (14x14px)
```

## Technical Details

### Component Architecture

- **C++ Component**: Handles API requests, data parsing, state management, and display rendering
- **ESPHome Integration**: Python code for YAML configuration and component setup
- **Display Library**: Uses HUB75 Matrix Display component
- **HTTP Client**: Built-in ESPHome `http_request` component

### State Machine

```
SCHEDULED → TODAY_PENDING → IN_PROGRESS → FINISHED → SCHEDULED
    ↑                                                      ↓
    └──────────────────────────────────────────────────────┘
```

Transitions:
- `SCHEDULED → TODAY_PENDING`: When current date matches match date
- `TODAY_PENDING → IN_PROGRESS`: At match start time (verified by API)
- `IN_PROGRESS → FINISHED`: When API reports match finished
- `FINISHED → SCHEDULED`: After 1 hour, fetch next match

### Performance

- **Memory Usage**: ~50KB RAM for component state and HTTP buffers
- **CPU Usage**: Minimal, updates only once per second
- **Network**: ~1KB per API request, one request every 5 minutes

## Contributing

To extend this component:

1. **Add new leagues**: Include logo files and update team_logos map
2. **Enhance display**: Modify drawing methods for additional information
3. **Add statistics**: Extend API parsing to include player stats, league tables, etc.
4. **Multiple teams**: Fork component to track multiple teams on different pages

## License

This component follows the same license as the Transit Tracker project.

## Credits

- Built using [ESPHome](https://esphome.io/)
- Data provided by [football-data.org](https://www.football-data.org/)
- Team logos from FootballLogos.org
- Inspired by the [Transit Tracker](https://github.com/EastsideUrbanism/transit-tracker) project

## Support

For issues and questions:
1. Check the troubleshooting section above
2. Review ESPHome logs: `esphome logs soccer-tracker.yaml`
3. Verify API responses manually
4. Open an issue with logs and configuration details
