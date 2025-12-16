# Soccer Tracker - Quick Start Guide

## What You Need

1. **Hardware**: Matrix Portal S3 + 64x32 (or 128x32) RGB LED Matrix
2. **API Subscription**: Paid API-Football plan (~$5-10/month for current season access)
3. **Team ID**: See [TEAM_IDS.md](TEAM_IDS.md) for MLS teams

## 5-Minute Setup

### Step 1: Subscribe to API-Football
```
1. Go to: https://www.api-football.com/
2. Purchase a paid subscription (~$5-10/month)
   ⚠️ Free tier only supports historical seasons (2021-2023)
   ✅ Paid tier: Live 2025 MLS + all major leagues
3. Copy your API key from the dashboard (Settings → API Key)
```

### Step 2: Create secrets.yaml
```bash
cd firmware
cp secrets.yaml.template secrets.yaml
```

Edit `secrets.yaml`:
```yaml
wifi_ssid: "YourWiFiName"
wifi_password: "YourWiFiPassword"
api_football_api_key: "paste_your_paid_api_key_here"
```

### Step 3: Configure Your Team

Edit `soccer-tracker.yaml` (lines 268-269):
```yaml
soccer_tracker:
  favorite_team: "Seattle Sounders FC"  # ← Change this
  team_id: 1659  # ← And this (see TEAM_IDS.md for other MLS teams)
```

### Step 4: Flash to Device
```bash
esphome run soccer-tracker.yaml
```

Select your COM port when prompted.

## Display Modes

Your display will automatically show:

**📅 Before Match Day**
```
[Logo] Team Name          12-25-24
[Logo] Opponent Name      19:00
```

**⏰ Match Day (Before Start)**
```
[Logo] Team Name          02:30  ← Hours:Minutes until kickoff
[Logo] Opponent Name             (colon pulses every second)
```

**⚽ Live Match**
```
[Logo] Team Name     2    45:23  ← Score + Match time
[Logo] Opponent Name 1           (colon pulses)
```

**🏁 Match Finished**
```
[Logo] Team Name     2    F      ← Final score + "F"
[Logo] Opponent Name 1
```

## Troubleshooting

### Display shows "Loading..."
- Wait 5-10 seconds for first API fetch
- Check WiFi is connected (press Up button to see IP page)
- Verify API key in `secrets.yaml` is correct from api-football.com dashboard

### Wrong team showing
- Check `team_id` matches your team (see TEAM_IDS.md)
- Verify `favorite_team` exactly matches API team name
- Note: API-Football uses different team IDs than football-data.org

### Logo not appearing
- Logo filename must match team name pattern
- Check `team_logos` map in YAML has correct mappings
- MLS teams should work out-of-the-box

### No upcoming matches
- Ensure it's within MLS season (roughly March-December)
- API-Football includes MLS, Premier League, La Liga, Bundesliga, Serie A, Ligue 1, etc.
- Check https://www.api-football.com for current league schedule

## Buttons

- **Up Button (GPIO 6)**: Switch to IP address page
- **Down Button (GPIO 7)**: Cycle brightness (5 levels: Full → 75% → 50% → 25% → Off → repeat in reverse)

## Advanced Configuration

### Change Update Frequency

Edit `soccer_tracker.cpp`:
```cpp
static constexpr unsigned long FETCH_INTERVAL = 300000; // 5 min (default)
```

### Add More Teams

1. Get 14x14 PNG logo
2. Add to `logos/teams_resized/`
3. Add image entry in YAML
4. Register in `team_logos` map

### Multiple Pages

Add more pages in `display:` section:
```yaml
display:
  pages:
    - id: soccer_page
      lambda: id(soccer).draw_match();
    - id: clock_page
      lambda: |-
        // Your clock code here
```

## API Limits

Paid subscription plan:
- ✅ Unlimited/very high request limits (depends on plan tier)
- ✅ All competitions including MLS, Premier League, La Liga, Bundesliga, Serie A, Ligue 1
- ✅ Full match data and statistics
- ✅ Current 2025 season live data

This component:
- Makes 1 request every 5 minutes
- Updates display every second (no API calls)
- Perfect for any paid plan tier

## Files Created

```
firmware/
├── soccer-tracker.yaml              ← Main config (start here)
├── secrets.yaml.template            ← Template for your secrets
├── SOCCER_TRACKER_README.md         ← Full documentation
├── TEAM_IDS.md                      ← Team ID reference
└── components/
    └── soccer_tracker/
        ├── __init__.py              ← ESPHome integration
        ├── soccer_tracker.h         ← C++ header
        └── soccer_tracker.cpp       ← C++ implementation
```

## Next Steps

1. ✅ Flash and verify it works
2. 📝 Customize your team in the YAML
3. 🎨 Adjust brightness with buttons
4. 📚 Read [SOCCER_TRACKER_README.md](SOCCER_TRACKER_README.md) for advanced features

## Support

- ESPHome logs: `esphome logs soccer-tracker.yaml`
- Check WiFi/API connectivity first
- Verify team ID matches football-data.org
- Review [SOCCER_TRACKER_README.md](SOCCER_TRACKER_README.md) troubleshooting section

---

**Enjoy tracking your team! ⚽🎉**
