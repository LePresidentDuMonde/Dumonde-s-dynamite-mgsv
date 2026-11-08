package masterserver

import (
	"database/sql"
	"log/slog"
	"net"
	"time"

	"github.com/unknown321/fuse/playerresource"
	"github.com/unknown321/fuse/server"
	"github.com/unknown321/fuse/sessionmanager"
	_ "modernc.org/sqlite"
)

const DefaultClientAddress = "http://127.0.0.1:6667/tppstm/gate"
const baseURL = "http://127.0.0.1:6667/"
const listenAddress = "127.0.0.1:6667"
const platform = "tppstm"

const dsnURI = "./dynamite/dynamite.dat"

// GATE and WEB url versions the client requires in CMD_GET_URLLIST: 17 for 1.0.15.3, 18 for 1.0.15.4
const urlListVersion = 18

func Run() {
	slog.Info("Starting master server")

	bonus := sessionmanager.SignupBonus{
		GMP: 25_000_000,
		Resources: playerresource.PlayerResource{
			Raw: playerresource.Raw{
				Fuel:          500_000,
				Bio:           500_000,
				CommonMetal:   500_000,
				MinorMetal:    500_000,
				PreciousMetal: 500_000,
			},
			Processed: playerresource.Processed{
				Fuel:          500_000,
				Bio:           500_000,
				CommonMetal:   500_000,
				MinorMetal:    500_000,
				PreciousMetal: 500_000,
			},
			Plants: playerresource.Plants{
				Wormwood:          10_000,
				BlackCarrot:       10_000,
				GoldenCrescent:    10_000,
				Tarragon:          10_000,
				AfricanPeach:      10_000,
				DigitalisPurpurea: 10_000,
				DigitalisLutea:    10_000,
				Haoma:             10_000,
			},
			Vehicles: playerresource.Vehicles{
				ZaAZS84:       100,
				APET41LV:      100,
				ZiGRA6T:       100,
				Boar53CT:      100,
				ZhukBr3:       100,
				StoutIfvSc:    100,
				ZhukRsZo:      100,
				StoutIfvFs:    100,
				TT77Nosorog:   100,
				M84AMagloader: 100,
			},
			WalkerGears: playerresource.WalkerGears{
				WGPP:        50,
				CCCPWGTypeC: 50,
				CCCPWGTypeA: 50,
				CFAWGTypeC:  50,
				CFAWGTypeA:  50,
			},
			Nuclear: playerresource.Nuclear{
				Weapon: 10,
				Waste:  10,
			},
			Parasites: playerresource.Parasites{
				Mist:       20,
				Camouflage: 20,
				Armor:      20,
			},
			Mystery01: 0,
			Placed: playerresource.Placed{
				VolgaK12:     100,
				HMG3Wingate:  100,
				M2A304Mortar: 100,
				Zhizdra45:    100,
				M276AAGGun:   100,
			},
			Mystery02: [7]int{},
			Unused:    [13]int{},
		},
	}

	go server.Start(baseURL, listenAddress, platform, false, false, dsnURI, bonus)
	go setURLListVersion()
}

// setURLListVersion overrides the url versions fuse writes on every start, client refuses to go online otherwise
func setURLListVersion() {
	for i := 0; ; i++ {
		c, err := net.DialTimeout("tcp", listenAddress, time.Second)
		if err == nil {
			_ = c.Close()
			break
		}
		if i == 60 {
			slog.Error("master server did not start, url list version not set")
			return
		}
		time.Sleep(time.Second)
	}

	db, err := sql.Open("sqlite", dsnURI)
	if err != nil {
		slog.Error("set url list version", "error", err.Error())
		return
	}
	defer db.Close()

	if _, err = db.Exec(`UPDATE urlListEntry SET version = ? WHERE urlType IN ('GATE', 'WEB')`, urlListVersion); err != nil {
		slog.Error("set url list version", "error", err.Error())
		return
	}
	slog.Info("url list version set", "version", urlListVersion)
}
