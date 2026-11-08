package handlers

import (
	"dynamite/config"
	"dynamite/util"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

const steamID64Base = 76561197960265728

type Friend struct {
	Name    string
	SteamID uint64
}

// vdfTokens splits Valve KeyValues text into quoted strings and braces
func vdfTokens(s string) []string {
	var tokens []string
	for i := 0; i < len(s); i++ {
		switch s[i] {
		case '{', '}':
			tokens = append(tokens, string(s[i]))
		case '"':
			var b strings.Builder
			for i++; i < len(s) && s[i] != '"'; i++ {
				if s[i] == '\\' && i+1 < len(s) {
					i++
				}
				b.WriteByte(s[i])
			}
			tokens = append(tokens, b.String())
		}
	}
	return tokens
}

// GetSteamFriends reads the friend list Steam caches in <accountDir>/config/localconfig.vdf
func GetSteamFriends(accountDir string) ([]Friend, error) {
	b, err := os.ReadFile(filepath.Join(accountDir, "config", "localconfig.vdf"))
	if err != nil {
		return nil, err
	}

	self := filepath.Base(accountDir)
	t := vdfTokens(string(b))
	var friends []Friend

	// find top level "friends" block: "UserLocalConfigStore" { "friends" { ... } }
	depth := 0
	for i := 0; i < len(t)-1; i++ {
		switch t[i] {
		case "{":
			depth++
			continue
		case "}":
			depth--
			continue
		}
		if depth != 1 || t[i] != "friends" || t[i+1] != "{" {
			continue
		}

		// inside friends: "<accountID>" { ... "name" "<name>" ... }
		d := 0
		id := ""
		for j := i + 1; j < len(t); j++ {
			switch {
			case t[j] == "{":
				d++
			case t[j] == "}":
				d--
				if d == 0 {
					return sortFriends(friends), nil
				}
			case d == 1 && j+1 < len(t) && t[j+1] == "{":
				id = t[j]
			case d == 2 && t[j] == "name" && j+1 < len(t) && id != "" && id != self:
				if acc, err := strconv.ParseUint(id, 10, 32); err == nil {
					friends = append(friends, Friend{Name: t[j+1], SteamID: acc + steamID64Base})
				}
				id = ""
				j++
			}
		}
	}

	return sortFriends(friends), nil
}

func sortFriends(f []Friend) []Friend {
	sort.Slice(f, func(i, j int) bool { return strings.ToLower(f[i].Name) < strings.ToLower(f[j].Name) })
	return f
}

func whitelisted(sid uint64) bool {
	for _, v := range Config.Coop.Whitelist {
		if e, err := config.FromString(v); err == nil && e.SteamID == sid {
			return true
		}
	}
	return false
}

// Quick saves host/join choice only and starts install, session page launches the game
func Quick(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	if err := Config.Load(); err != nil {
		http.Error(w, fmt.Sprintf("load config: %s", err.Error()), http.StatusInternalServerError)
		return
	}

	friend := strings.TrimSpace(r.FormValue("friend"))
	if friend == "" {
		friend = strings.TrimSpace(r.FormValue("friendManual"))
	}
	var sid uint64
	if friend != "" {
		var err error
		if sid, err = util.GetSteamID(friend); err != nil || sid == 0 {
			http.Error(w, "Invalid friend Steam ID / profile link: "+friend, http.StatusBadRequest)
			return
		}
	}

	switch r.FormValue("mode") {
	case "host":
		Config.Coop.Host = true
		// whitelist empty means everyone may join; otherwise make sure the chosen friend can
		if sid != 0 && len(Config.Coop.Whitelist) > 0 && !whitelisted(sid) {
			Config.Coop.Whitelist = append(Config.Coop.Whitelist, config.LimitListEntry{SteamID: sid, Note: "added by quick play"}.String())
		}
	case "join":
		if sid == 0 {
			http.Error(w, "Pick a friend or paste their Steam ID / profile link", http.StatusBadRequest)
			return
		}
		Config.Coop.Host = false
		Config.Coop.HostSteamID = sid
	default:
		http.Error(w, "Choose Host or Join", http.StatusBadRequest)
		return
	}

	if Config.Dynamite.AccountDir == "" {
		http.Error(w, "Set your Steam account in the Configuration section below first", http.StatusBadRequest)
		return
	}

	if err := Config.Save(); err != nil {
		http.Error(w, fmt.Sprintf("Failed to save config: %s", err.Error()), http.StatusInternalServerError)
		return
	}

	InstallStat.Done = false
	InstallStat.Error = ""
	select {
	case StartInstallChan <- true:
	default: // install already queued
	}

	http.Redirect(w, r, "/session?launch=1", http.StatusSeeOther)
}
