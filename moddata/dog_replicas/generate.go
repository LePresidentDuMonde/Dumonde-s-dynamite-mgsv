//go:build ignore

package main

import (
	"bytes"
	"fmt"
	"github.com/unknown321/datfpk/fox2"
	"github.com/unknown321/datfpk/util"
	"os"
)

func main() {
	for variant := 0; variant < 5; variant++ {
		name := fmt.Sprintf("moddata/dog_replicas/buddy_dog2_%02d.fox2", variant)
		if len(os.Args) > 1 && os.Args[1] == "-import" {
			source, err := os.ReadFile(name)
			if err != nil {
				panic(err)
			}
			var scene fox2.Fox2
			if err = scene.Read(bytes.NewReader(source)); err != nil {
				panic(err)
			}
			var xml bytes.Buffer
			if err = scene.ToXML(&xml); err != nil {
				panic(err)
			}
			if err = os.WriteFile(name+".xml", xml.Bytes(), 0644); err != nil {
				panic(err)
			}
			continue
		}
		source, err := os.ReadFile(name + ".xml")
		if err != nil {
			panic(err)
		}
		var scene fox2.Fox2
		if err = scene.FromXML(bytes.NewReader(source)); err != nil {
			panic(err)
		}
		out := util.NewByteArrayReaderWriter(nil)
		if err = scene.Write(out); err != nil {
			panic(err)
		}
		// The same writer is used by the existing player scene generator.
		if err = os.WriteFile(name, out.Bytes(), 0644); err != nil {
			panic(err)
		}
		fmt.Println("generated two-Dog scene", name)
	}
}
