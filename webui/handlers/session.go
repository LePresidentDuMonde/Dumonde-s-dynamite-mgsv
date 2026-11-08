package handlers

import (
	"encoding/json"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"
)

var logPath = filepath.Join("dynamite", "dynamite.log.txt")

const logTailBytes = 64 * 1024

type sessionState struct {
	Role    string   `json:"role"`
	Status  string   `json:"status"`
	Hint    string   `json:"hint"`
	Level   string   `json:"level"` // ok, info, warn, error
	Age     int64    `json:"age"`   // seconds since last log write, -1 if no log
	Lines   []string `json:"lines"`
	Verbose bool     `json:"verbose"`

	Installing   bool   `json:"installing"`
	InstallError string `json:"installError"`
}

// rules are checked in order for every log line, last matching line wins
var sessionRules = []struct {
	match, status, hint, level string
}{
	{"] starting", "Game started, mod loaded", "", "info"},
	{"host session res: 1", "Hosting: session open", "Waiting for players to join. They must use your Steam ID as Host Steam ID.", "ok"},
	{"Connection request:", "A player is trying to join", "", "info"},
	{"Connection accepted", "Join request accepted, waiting for the game handshake", "If the player times out here, enable network debugging on both sides and send both logs.", "info"},
	{"Not in whitelist", "Rejected a player: not in whitelist", "Add their Steam ID to the whitelist, or empty it.", "warn"},
	{"In blacklist", "Rejected a player: blacklisted", "", "warn"},
	{"dynamite_host_connect_attempt", "You are in host mode", "Turn Host Mode off in Configuration to join someone.", "warn"},
	{"dynamite_establishing_connection", "Connecting to host...", "The host must be running the game with Host Mode on.", "info"},
	{"dynamite_connection_failed", "Game refused to start the connection", "Retrying automatically. If it keeps failing, go back to the helicopter and try again.", "error"},
	{"session already created", "Previous connection attempt still active", "It resets on disconnect or mission end.", "warn"},
	{"Co-op connection established", "Connected to host", "", "ok"},
	{"disconnect from host", "Connection timed out or host disconnected", "Check that the host is in game with Host Mode on and that you are in their whitelist.", "error"},
}

func readLogTail() ([]string, int64, error) {
	f, err := os.Open(logPath)
	if err != nil {
		return nil, -1, err
	}
	defer f.Close()

	st, err := f.Stat()
	if err != nil {
		return nil, -1, err
	}

	off := st.Size() - logTailBytes
	if off < 0 {
		off = 0
	}

	b, err := io.ReadAll(io.NewSectionReader(f, off, st.Size()-off))
	if err != nil {
		return nil, -1, err
	}

	lines := strings.Split(strings.TrimRight(strings.ReplaceAll(string(b), "\r\n", "\n"), "\n"), "\n")
	if off > 0 && len(lines) > 0 {
		lines = lines[1:] // first line is likely cut
	}

	return lines, int64(time.Since(st.ModTime()).Seconds()), nil
}

func parseSession(lines []string) sessionState {
	s := sessionState{
		Role:   "unknown",
		Status: "Game not started yet",
		Hint:   "Launch the game from Steam. This page updates by itself.",
		Level:  "info",
	}

	start := 0
	for i, l := range lines {
		if strings.HasSuffix(l, "] starting") {
			start = i // only the latest game run matters
		}
	}

	for _, l := range lines[start:] {
		switch {
		case strings.HasSuffix(l, "Host: true"):
			s.Role = "host"
		case strings.HasSuffix(l, "Host: false"):
			s.Role = "client"
		case strings.Contains(l, "debug.Nio: true"):
			s.Verbose = true
		}

		for _, r := range sessionRules {
			if strings.Contains(l, r.match) {
				s.Status, s.Hint, s.Level = r.status, r.hint, r.level
				break
			}
		}
	}

	s.Lines = lines[start:]
	return s
}

func SessionAPI(w http.ResponseWriter, r *http.Request) {
	lines, age, err := readLogTail()
	s := parseSession(lines)
	s.Age = age
	s.Installing = !InstallStat.Done
	s.InstallError = InstallStat.Error
	if err != nil && !os.IsNotExist(err) {
		s.Status, s.Hint, s.Level = "Cannot read mod log", err.Error(), "error"
	}

	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	_ = json.NewEncoder(w).Encode(s)
}

func Session(w http.ResponseWriter, r *http.Request) {
	p := pageData{Version: &Version, HasUpdate: HasUpdate, Flavor: currentFlavor}
	if err := tmpl.ExecuteTemplate(w, "session.tmpl", p); err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}
}
