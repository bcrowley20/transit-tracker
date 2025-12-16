# Soccer Tracker Implementation Summary

## Overview

A complete ESPHome component for displaying live soccer match information on a Matrix Portal S3 with HUB75 RGB LED matrix display. The component integrates with the football-data.org API to fetch match data and implements a state machine with four display modes.

## Architecture

### Component Structure

```
soccer_tracker/
├── __init__.py              # ESPHome Python integration
├── soccer_tracker.h         # C++ header with class definition
└── soccer_tracker.cpp       # C++ implementation
```

### Key Design Decisions

#### 1. ESPHome HTTP Request Component
**Decision**: Use ESPHome's built-in `http_request` component rather than implementing custom HTTP client.

**Rationale**:
- Leverages existing, tested HTTP functionality
- Handles SSL/TLS certificates automatically
- Integrates seamlessly with ESPHome's async architecture
- Reduces code complexity and maintenance burden

**Implementation**:
```cpp
this->http_request_->get(url, headers, [this](response) {
    // Async callback processes response
});
```

#### 2. State Machine for Display Modes
**Decision**: Implement explicit state enum rather than implicit state tracking.

**States**:
- `SCHEDULED` - Match not today
- `TODAY_PENDING` - Match today, not started
- `IN_PROGRESS` - Match currently being played
- `FINISHED` - Match ended (show for 1 hour)

**Rationale**:
- Clear separation of concerns for each mode
- Easy to extend with additional states
- Simplifies transition logic
- Makes debugging easier

#### 3. Team Name Normalization
**Decision**: Implement fuzzy matching between API team names and logo filenames.

**Algorithm**:
```cpp
1. Extract team name from logo filename
2. Normalize both API name and logo name:
   - Convert to lowercase
   - Replace spaces with hyphens
   - Handle special cases ("FC", "SC", etc.)
3. Perform substring matching in both directions
```

**Rationale**:
- API may return different team name formats
- Logo filenames follow consistent pattern
- Allows for minor variations without manual mapping

#### 4. Update Frequency
**Decisions**:
- API fetch: Every 5 minutes
- Display update: Every 1 second
- Colon pulse: Toggle every second

**Rationale**:
- API rate limits (10 req/min on free tier)
- Match data doesn't change that frequently
- Second-by-second display updates for smooth countdown/timer
- Visual heartbeat indicator for live matches

## Technical Implementation

### HTTP Request Flow

```
setup() → Schedule initial fetch (5s delay)
    ↓
fetch_match_data_()
    ↓
Build API URL with team_id
    ↓
Add X-Auth-Token header
    ↓
Async GET request
    ↓
Callback: parse_match_response_()
    ↓
Update current_match_ state
    ↓
has_match_data_ = true
    ↓
loop() continues to call draw_match()
```

### Display Rendering

```
draw_match()
    ↓
Check match state
    ↓
┌─────────────┬──────────────────┬────────────────┬──────────────┐
│  SCHEDULED  │  TODAY_PENDING   │  IN_PROGRESS   │   FINISHED   │
├─────────────┼──────────────────┼────────────────┼──────────────┤
│draw_team    │draw_team         │draw_team       │draw_team     │
│draw_team    │draw_team         │draw_team       │draw_team     │
│draw_date    │draw_countdown    │draw_score      │draw_score    │
│draw_time    │(pulsing colon)   │draw_time_match │"F" indicator │
└─────────────┴──────────────────┴────────────────┴──────────────┘
```

### State Transitions

```
                    ┌──────────────┐
         ┌─────────→│  SCHEDULED   │←────────┐
         │          └──────┬───────┘         │
         │                 │ Same day?       │
         │                 ↓ Yes             │
         │          ┌──────────────┐         │
         │          │TODAY_PENDING │         │
         │          └──────┬───────┘         │
         │                 │ Match start?    │
         │                 ↓ Yes             │
         │          ┌──────────────┐         │
         │          │ IN_PROGRESS  │         │
         │          └──────┬───────┘         │
         │                 │ Match end?      │
         │                 ↓ Yes             │
         │          ┌──────────────┐         │
         └──────────┤   FINISHED   │         │
            After   └──────────────┘         │
            1 hour         │                 │
                          │ Fetch next       │
                          └──────────────────┘
```

## API Integration

### Endpoint Used
```
GET https://api.football-data.org/v4/teams/{team_id}/matches
    ?status=SCHEDULED,LIVE,FINISHED
    &limit=10
```

### Response Parsing

**Key Fields**:
- `utcDate` - Match date/time in ISO 8601 format
- `status` - Match status (SCHEDULED, IN_PLAY, FINISHED, etc.)
- `homeTeam.name` / `awayTeam.name` - Team names
- `score.fullTime.home` / `score.fullTime.away` - Current/final scores
- `minute` - Current match minute (if available)

**ISO 8601 Parsing**:
```cpp
// Custom parser for "YYYY-MM-DDTHH:MM:SSZ" format
parse_iso8601(datetime_str, result_time_t)
```

**Rationale**: 
- `strptime` not available on all platforms
- Simple sscanf-based parser is more portable
- Handles UTC timestamps consistently

## Display Layout

### 128x32 Pixel Display (2 × 64x32 chained)

```
┌────────────────────────────────────────────────────────────────┐
│ Row 1 (0-15px):  [Logo] Favorite Team      Right Data          │
│ Row 2 (16-31px): [Logo] Opponent Team      Right Data          │
└────────────────────────────────────────────────────────────────┘

Left Side (~0-80px):
  - Team logo: 14×14px at x=0, y=0 or y=16
  - 2px gap
  - Team name: x=16, size 8px font

Right Side (~80-128px):
  - Mode-specific data (date, countdown, score, time)
  - Aligned to right edge
  - Small font (6px) for date/time
  - Large font (8px) for scores
```

### Font Selection

**Primary Font (8px)**: Team names, scores, "F" indicator
- `Pixolletta8px` at size 8
- Monospace-style for consistent alignment

**Small Font (6px)**: Dates, times, countdown
- `Pixolletta8px` at size 6
- Fits more characters in limited space

## Logo Management

### Logo Specifications
- **Size**: 14×14 pixels
- **Format**: PNG
- **Color**: RGB565 (for ESPHome image component)
- **Naming**: `{team-name}_footballlogos-org_14x14.png`

### Logo Registration

**YAML Configuration**:
```yaml
image:
  - file: "path/to/logo.png"
    id: team_logo_id
    type: RGB565

soccer_tracker:
  team_logos:
    "logo-filename.png": team_logo_id
```

**Runtime Lookup**:
```cpp
get_team_logo_(team_name)
  → normalize_team_name_(team_name)
  → search team_logos_ map
  → return Image* or nullptr
```

## Error Handling

### Network Errors
- Check `network::is_connected()` before fetching
- Log warnings for failed requests
- Continue with last known good data

### API Errors
- Log HTTP status codes
- Handle empty responses gracefully
- Parse JSON defensively with null checks

### Missing Data
- Display "Loading..." if no match data yet
- Handle missing logos (show team name only)
- Fall back to estimated match time if minute unavailable

## Performance Characteristics

### Memory Usage
- **Component state**: ~2KB
- **HTTP response buffer**: ~16KB (temporary)
- **JSON parsing**: ~8KB (temporary)
- **Display buffer**: Managed by hub75_matrix_display component
- **Total**: ~30KB RAM typical usage

### CPU Usage
- **API fetch**: Brief spike every 5 minutes
- **Display update**: Minimal, once per second
- **Most time**: Idle in loop() polling

### Network Usage
- **Typical response**: 1-3KB per request
- **Frequency**: Once every 5 minutes
- **Daily**: ~0.5MB per day

## Extensibility

### Adding New Display Modes

1. Add state to `MatchState` enum
2. Implement `draw_new_mode_()` method
3. Add case to `draw_match()` switch
4. Update state transition logic in `update_match_state_()`

### Supporting Multiple Teams

1. Convert `current_match_` to vector
2. Add team selection button handler
3. Fetch matches for multiple team IDs
4. Display pages for each team

### Adding Statistics

1. Parse additional JSON fields from API
2. Add members to `Team` or `Match` struct
3. Create new drawing methods
4. Add optional display page

## Testing Recommendations

### Unit Testing
- Mock HTTP responses with known JSON
- Test state transitions with fixed timestamps
- Verify team name normalization edge cases

### Integration Testing
- Test with real API (development key)
- Verify all four display modes
- Check logo matching for various team names
- Test timezone handling

### Hardware Testing
- Verify display brightness levels
- Test button responsiveness
- Confirm WiFi reconnection handling
- Long-term stability (24+ hours)

## Known Limitations

1. **Free API Tier**: Limited to specific competitions
2. **Logo Coverage**: Only includes MLS teams by default
3. **Timezone**: Configured globally, not per-match
4. **Single Match**: Shows only next/current match for one team
5. **Text Length**: Long team names may be truncated

## Future Enhancements

### Potential Improvements
- [ ] Multiple team tracking
- [ ] League standings display
- [ ] Player statistics
- [ ] Custom color schemes per team
- [ ] Match history view
- [ ] Notification sounds for goals
- [ ] Integration with Home Assistant

### Performance Optimizations
- [ ] Cache logos in PSRAM
- [ ] Incremental JSON parsing
- [ ] Differential display updates
- [ ] Predictive API fetching

## Dependencies

### ESPHome Components
- `http_request` - HTTP client
- `json` - JSON parsing
- `network` - Network status
- `display` - Display abstraction
- `font` - Font rendering
- `time` - RTC synchronization
- `hub75_matrix_display` - HUB75 LED matrix driver

### C++ Standard Library
- `<ctime>` - Time manipulation
- `<algorithm>` - String algorithms
- `<cctype>` - Character classification
- `<map>` - Logo registry
- `<string>` - String operations

## Lessons Learned

1. **Async Operations**: ESPHome's async patterns require careful callback handling
2. **Platform Portability**: Avoid platform-specific functions like `strptime`
3. **API Rate Limits**: Build in delays and respect free tier limitations
4. **Error Recovery**: Degrade gracefully when API unavailable
5. **Logo Management**: Automated mapping reduces manual configuration

## Conclusion

The Soccer Tracker component demonstrates a complete ESPHome custom component implementation, integrating:
- External REST API consumption
- State machine-based display logic
- Resource management (logos, fonts)
- Real-time updates with controlled fetching
- Hardware button integration

The modular design allows for easy extension while maintaining code clarity and performance within the constraints of an ESP32 microcontroller.
