# Football-Data.org Team IDs Reference

## Major League Soccer (MLS) Teams

| Team Name | Team ID | Logo File |
|-----------|---------|-----------|
| Atlanta United FC | 1032 | atlanta-united-footballlogos-org_14x14.png |
| Austin FC | 1059 | austin-fc-footballlogos-org_14x14.png |
| CF Montréal | 1029 | cf-montreal-footballlogos-org_14x14.png |
| Charlotte FC | 1054 | charlotte-fc-footballlogos-org_14x14.png |
| Chicago Fire FC | 1026 | chicago-fire-footballlogos-org_14x14.png |
| Colorado Rapids | 1027 | colorado-rapids-footballlogos-org_14x14.png |
| Columbus Crew | 1025 | columbus-crew-footballlogos-org_14x14.png |
| D.C. United | 1020 | dc-united-footballlogos-org_14x14.png |
| FC Cincinnati | 1045 | fc-cincinnati-footballlogos-org_14x14.png |
| FC Dallas | 1024 | fc-dallas-footballlogos-org_14x14.png |
| Houston Dynamo FC | 1028 | houston-dynamo-footballlogos-org_14x14.png |
| Inter Miami CF | 1053 | inter-miami-footballlogos-org_14x14.png |
| LA Galaxy | 1023 | los-angeles-galaxy-footballlogos-org_14x14.png |
| Los Angeles FC | 1040 | los-angeles-fc-footballlogos-org_14x14.png |
| Minnesota United FC | 1039 | minnesota-united-footballlogos-org_14x14.png |
| Nashville SC | 1052 | nashville-sc-footballlogos-org_14x14.png |
| New England Revolution | 1019 | new-england-revolution-footballlogos-org_14x14.png |
| New York City FC | 1036 | new-york-city-fc-footballlogos-org_14x14.png |
| New York Red Bulls | 1021 | new-york-red-bulls-footballlogos-org_14x14.png |
| Orlando City SC | 1037 | orlando-city-footballlogos-org_14x14.png |
| Philadelphia Union | 1033 | philadelphia-union-footballlogos-org_14x14.png |
| Portland Timbers | 1031 | portland-timbers-footballlogos-org_14x14.png |
| Real Salt Lake | 1030 | real-salt-lake-footballlogos-org_14x14.png |
| San Diego FC | 1070 | san-diego-fc-footballlogos-org_14x14.png |
| San Jose Earthquakes | 1022 | san-jose-earthquakes-footballlogos-org_14x14.png |
| Seattle Sounders FC | 1035 | seattle-sounders-footballlogos-org_14x14.png |
| Sporting Kansas City | 1034 | sporting-kansas-city-footballlogos-org_14x14.png |
| St. Louis City SC | 1060 | st-louis-city-footballlogos-org_14x14.png |
| Toronto FC | 1038 | toronto-fc-footballlogos-org_14x14.png |
| Vancouver Whitecaps FC | 1041 | vancouver-whitecaps-footballlogos-org_14x14.png |

## How to Find Team IDs

### Method 1: API Explorer
Visit the football-data.org API documentation and use the team endpoint:
```
https://api.football-data.org/v4/teams/{id}
```

### Method 2: Competition Teams
Get all teams in a competition:
```
https://api.football-data.org/v4/competitions/{competition}/teams
```

Common competition codes:
- `MLS` - Major League Soccer
- `PL` - Premier League
- `PD` - La Liga
- `BL1` - Bundesliga
- `SA` - Serie A
- `FL1` - Ligue 1
- `CL` - Champions League

### Method 3: Search by Name
Use the API to search for a specific team:
```bash
curl -X GET \
  'https://api.football-data.org/v4/teams?name=Seattle' \
  -H 'X-Auth-Token: YOUR_API_KEY'
```

## Notes

1. **Team IDs are unique** across all competitions and leagues
2. **Free tier limitations**: The free API tier may not have access to all competitions
3. **Team names must match** the exact name returned by the API for logo matching to work
4. **Logo files** should be 14x14 pixels in PNG format with RGB565 color encoding

## Adding New Teams

To add a team not in the MLS list:

1. Find the team ID using one of the methods above
2. Download or create a 14x14 pixel PNG logo
3. Name it: `{team-name}_footballlogos-org_14x14.png`
4. Add to `logos/teams_resized/` directory
5. Add image entry in YAML:
   ```yaml
   - file: "path/to/logo.png"
     id: team_logo_id
     type: RGB565
   ```
6. Register in team_logos map:
   ```yaml
   team_logos:
     "{team-name}_footballlogos-org_14x14.png": team_logo_id
   ```

## API Response Format

Example team data from API:
```json
{
  "id": 1035,
  "name": "Seattle Sounders FC",
  "shortName": "Sounders",
  "tla": "SEA",
  "crest": "https://crests.football-data.org/1035.png",
  "founded": 2007,
  "venue": "Lumen Field"
}
```

The component uses:
- `id` - To fetch team-specific match data
- `name` - To match with your favorite_team configuration
- Match with logo filenames for display
