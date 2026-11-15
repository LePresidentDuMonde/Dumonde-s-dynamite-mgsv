package moddata

import (
	"bytes"
	"encoding/xml"
	"fmt"
	"github.com/unknown321/datfpk/fox2"
	"github.com/unknown321/datfpk/util"
	"strconv"
	"strings"
	"testing"
)

type dogSceneEntity struct {
	Class      string `xml:"class,attr"`
	Address    string `xml:"addr,attr"`
	ID         string `xml:"id,attr"`
	Properties []struct {
		Name   string `xml:"name,attr"`
		Type   string `xml:"type,attr"`
		Values []struct {
			Text string `xml:",chardata"`
			Data string `xml:"data"`
		} `xml:"containerEntry"`
	} `xml:"staticProperties>property"`
}

func TestDogReplicaSceneGraphs(t *testing.T) {
	for variant := 0; variant < 5; variant++ {
		t.Run(fmt.Sprint(variant), func(t *testing.T) {
			name := fmt.Sprintf("dog_replicas/buddy_dog2_%02d.fox2", variant)
			data, err := ModData.ReadFile(name)
			if err != nil {
				t.Fatal(err)
			}
			var scene fox2.Fox2
			if err = scene.Read(bytes.NewReader(data)); err != nil {
				t.Fatal(err)
			}
			// Check the actual embedded binary, then verify another binary round trip.
			var text bytes.Buffer
			if err = scene.ToXML(&text); err != nil {
				t.Fatal(err)
			}
			var graph struct {
				Entities []dogSceneEntity `xml:"entities>entity"`
			}
			if err = xml.Unmarshal(text.Bytes(), &graph); err != nil {
				t.Fatal(err)
			}
			addresses, ids := map[uint64]bool{}, map[uint64]bool{}
			parse := func(v string) uint64 {
				n, e := strconv.ParseUint(strings.TrimSpace(v), 0, 64)
				if e != nil {
					t.Fatal(e)
				}
				return n
			}
			for _, entity := range graph.Entities {
				address, id := parse(entity.Address), parse(entity.ID)
				if addresses[address] || ids[id] {
					t.Fatal("aliased locator/transform/parameter entity")
				}
				addresses[address], ids[id] = true, true
			}
			locators, pools, remote := 0, 0, 0
			for _, entity := range graph.Entities {
				if entity.Class == "GameObjectLocator" {
					locators++
				}
				if entity.Class == "GameObject" {
					pools++
				}
				for _, property := range entity.Properties {
					for _, value := range property.Values {
						v := strings.TrimSpace(value.Text)
						if value.Data != "" {
							v = value.Data
						}
						if entity.Class == "GameObject" && (property.Name == "totalCount" || property.Name == "realizedCount") && v != "2" {
							t.Fatal("second Dog was not allocated")
						}
						if property.Name == "name" && v == "BuddyDogGameObjectLocator_Remote" {
							remote++
						}
						if property.Type == "EntityPtr" || property.Type == "EntityHandle" {
							if target := parse(v); target != 0 && !addresses[target] {
								t.Fatal("dangling scene entity reference", v)
							}
						}
					}
				}
			}
			if locators != 2 || pools != 1 || remote != 1 {
				t.Fatal("missing independent remote locator", locators, pools, remote)
			}
			out := util.NewByteArrayReaderWriter(nil)
			if err = scene.Write(out); err != nil {
				t.Fatal(err)
			}
			var again fox2.Fox2
			if err = again.Read(bytes.NewReader(out.Bytes())); err != nil || len(again.Entities) != len(scene.Entities) {
				t.Fatal("scene did not survive binary round trip", err)
			}
			found := false
			for _, patch := range ReplaceInFPK {
				for _, entry := range patch.Replace {
					if entry.SourceFile == name {
						found = true
						if !strings.Contains(patch.Source.SourceQar, "chunk1.dat") {
							t.Fatal("wrong source archive")
						}
					}
				}
			}
			if !found {
				t.Fatal("scene was not wired into installation")
			}
		})
	}
}
