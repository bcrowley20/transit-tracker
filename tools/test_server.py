from datetime import datetime, timedelta, timezone
from flask import Flask, jsonify, request, render_template_string
import threading

app = Flask(__name__)

# In-memory fixture state mimicking API-Football v3 minimal fields used
TEAMS = [
    "Atlanta United",
    "Austin FC",
    "CF Montreal",
    "Charlotte FC",
    "Chicago Fire",
    "Colorado Rapids",
    "Columbus Crew",
    "DC United",
    "FC Cincinnati",
    "FC Dallas",
    "Houston Dynamo",
    "Inter Miami",
    "Los Angeles FC",
    "Los Angeles Galaxy",
    "Minnesota United",
    "Nashville SC",
    "New England Revolution",
    "New York City FC",
    "New York Red Bulls",
    "Orlando City",
    "Philadelphia Union",
    "Portland Timbers",
    "Real Salt Lake",
    "San Jose Earthquakes",
    "Seattle Sounders FC",
    "Sporting Kansas City",
    "St Louis City SC",
    "Toronto FC",
    "Vancouver Whitecaps FC",
]

state = {
    "team_id": 1595,
    "opponent": {
        "name": "Colorado Rapids",
    },
    "favorite": {
        "name": "Seattle Sounders FC",
    },
    "match_time": datetime.now(timezone.utc) + timedelta(days=2, hours=2),
    "status": "NS",  # NS, TODAY (custom), 1H, 2H, HT, FT
    "home_is_favorite": True,
    "home_goals": 0,
    "away_goals": 0,
}

HTML = """
<!doctype html>
<title>Soccer Tracker Test Server</title>
<style>
  body { font-family: system-ui, sans-serif; margin: 2rem; }
  .row { margin: 0.5rem 0; }
  button { margin-right: 0.5rem; }
  code { background: #f2f2f2; padding: 0.2rem 0.4rem; }
</style>
<h1>Soccer Tracker Test Server</h1>
<p>Current fixture:</p>
<ul>
  <li>Status: <code id="status">{{status}}</code></li>
  <li>Match Time (UTC): <code id="time">{{match_time}}</code></li>
  <li>Home: <code>{{home_name}}</code> (goals: <code id="hg">{{home_goals}}</code>)</li>
  <li>Away: <code>{{away_name}}</code> (goals: <code id="ag">{{away_goals}}</code>)</li>
</ul>

<div class="row">
    <strong>Select Teams:</strong><br/>
    Home:
    <select id="home_select" onchange="setTeam('home', this.value)">
        {% for t in teams %}
            <option value="{{t}}" {% if t==home_name %}selected{% endif %}>{{t}}</option>
        {% endfor %}
    </select>
    &nbsp;&nbsp;Away:
    <select id="away_select" onchange="setTeam('away', this.value)">
        {% for t in teams %}
            <option value="{{t}}" {% if t==away_name %}selected{% endif %}>{{t}}</option>
        {% endfor %}
    </select>
</div>

<div class="row">
  <strong>Set State:</strong>
  <button onclick="setState('NS')">Scheduled (NS)</button>
  <button onclick="setState('TODAY')">Today Pending</button>
  <button onclick="setState('1H')">In Progress (1H)</button>
  <button onclick="setState('HT')">Half Time (HT)</button>
  <button onclick="setState('2H')">In Progress (2H)</button>
  <button onclick="setState('FT')">Finished (FT)</button>
</div>

<div class="row">
  <strong>Adjust Time:</strong>
  <button onclick="shiftTime(-1)">-1 day</button>
  <button onclick="shiftTime(0)">Today</button>
  <button onclick="shiftTime(1)">+1 day</button>
</div>

<div class="row">
  <strong>Scores:</strong>
  <button onclick="goal('home')">Home +1</button>
  <button onclick="goal('away')">Away +1</button>
  <button onclick="resetScores()">Reset</button>
</div>

<script>
async function setState(s) {
  const res = await fetch('/set_state', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({status:s})});
  const j = await res.json();
  document.getElementById('status').innerText = j.status;
}
async function shiftTime(days) {
  const res = await fetch('/shift_time', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({days})});
  const j = await res.json();
  document.getElementById('time').innerText = j.match_time;
}
async function goal(side) {
  const res = await fetch('/goal', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({side})});
  const j = await res.json();
  document.getElementById('hg').innerText = j.home_goals;
  document.getElementById('ag').innerText = j.away_goals;
}
async function resetScores() {
  const res = await fetch('/reset_scores', {method:'POST'});
  const j = await res.json();
  document.getElementById('hg').innerText = j.home_goals;
  document.getElementById('ag').innerText = j.away_goals;
}

async function setTeam(which, name) {
    const res = await fetch('/set_team', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({which, name})});
    const j = await res.json();
}
</script>
"""


def iso(dt: datetime) -> str:
    return dt.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S+00:00")


def build_fixture_response():
    # Map the local UI status to API-Football status codes used by firmware
    status_map = {
        "NS": "NS",
        "TODAY": "NS",  # still NS, firmware decides TODAY vs SCHEDULED using local date
        "1H": "1H",
        "HT": "HT",
        "2H": "2H",
        "FT": "FT",
    }
    status = status_map.get(state["status"], "NS")

    # Determine which team is home/away based on flag
    home = state["favorite"] if state["home_is_favorite"] else state["opponent"]
    away = state["opponent"] if state["home_is_favorite"] else state["favorite"]

    return {
        "response": [
            {
                "fixture": {
                    "date": iso(state["match_time"]),
                    "status": status,
                },
                # API-Football includes a top-level goals object
                "goals": {
                    "home": state["home_goals"],
                    "away": state["away_goals"],
                },
                "teams": {
                    "home": {
                        "name": home["name"],
                        "goals": state["home_goals"],
                    },
                    "away": {
                        "name": away["name"],
                        "goals": state["away_goals"],
                    },
                },
            }
        ]
    }


@app.get("/")
def index():
    home = state["favorite"]["name"] if state["home_is_favorite"] else state["opponent"]["name"]
    away = state["opponent"]["name"] if state["home_is_favorite"] else state["favorite"]["name"]
    return render_template_string(
        HTML,
        status=state["status"],
        match_time=iso(state["match_time"]),
        home_name=home,
        away_name=away,
        home_goals=state["home_goals"],
        away_goals=state["away_goals"],
        teams=TEAMS,
    )


@app.get("/fixtures")
def fixtures():
    # Mimic API-Football endpoint used by firmware: /fixtures?team=1595&next=1
    return jsonify(build_fixture_response())


@app.post("/set_state")
def set_state():
    j = request.get_json(force=True)
    status = j.get("status", "NS")
    state["status"] = status
    return jsonify({"status": state["status"]})


@app.post("/shift_time")
def shift_time():
    j = request.get_json(force=True)
    days = int(j.get("days", 0))
    # Shift to midnight local equivalent but keep UTC timezone; firmware does local conversion
    state["match_time"] = datetime.now(timezone.utc).replace(hour=2, minute=0, second=0, microsecond=0) + timedelta(days=days)
    return jsonify({"match_time": iso(state["match_time"])})


@app.post("/goal")
def goal():
    j = request.get_json(force=True)
    side = j.get("side", "home")
    if side == "home":
        state["home_goals"] += 1
    else:
        state["away_goals"] += 1
    return jsonify({"home_goals": state["home_goals"], "away_goals": state["away_goals"]})


@app.post("/reset_scores")
def reset_scores():
    state["home_goals"] = 0
    state["away_goals"] = 0
    return jsonify({"home_goals": 0, "away_goals": 0})
@app.post("/set_team")
def set_team():
    j = request.get_json(force=True)
    which = j.get("which")
    name = j.get("name")
    if not name or which not in ("home","away"):
        return jsonify({"error":"invalid"}), 400
    # Update favorite/opponent names based on current home_is_favorite mapping
    if state["home_is_favorite"]:
        # favorite is home, opponent is away
        if which == "home":
            state["favorite"]["name"] = name
        else:
            state["opponent"]["name"] = name
    else:
        # opponent is home, favorite is away
        if which == "home":
            state["opponent"]["name"] = name
        else:
            state["favorite"]["name"] = name
    return jsonify({"ok": True})


@app.post("/toggle_home")
def toggle_home():
    state["home_is_favorite"] = not state["home_is_favorite"]
    return jsonify({"home_is_favorite": state["home_is_favorite"]})


if __name__ == "__main__":
    print("Test server running on http://0.0.0.0:5000 (listening on all interfaces)")
    print("Endpoints:")
    print("  GET  /fixtures        -> API-like response")
    print("  GET  /               -> Control UI")
    app.run(host="0.0.0.0", port=5000, debug=True)
