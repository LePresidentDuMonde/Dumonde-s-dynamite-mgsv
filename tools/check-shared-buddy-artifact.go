//go:build ignore

// Verifies that the launcher embeds the exact locally built DLL and current companion scripts.
package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"os"
)

func main() {
	artifact := flag.String("artifact", "dynamite-two-ddogs-experimental-windows-amd64.exe", "launcher")
	flag.Parse()
	data, err := os.ReadFile(*artifact)
	if err != nil {
		panic(err)
	}
	items := []map[string]any{}
	paths:=[]string{"hook/cmake-build-release-docker-mingw15/dinput8.dll", "moddata/standalone/DynamiteBuddy.lua", "moddata/standalone/DynamiteCoop.lua"}
	for variant:=0;variant<5;variant++ { paths=append(paths,fmt.Sprintf("moddata/dog_replicas/buddy_dog2_%02d.fox2",variant)) }
	for _, path := range paths {
		content, err := os.ReadFile(path)
		if err != nil {
			panic(err)
		}
		offset := bytes.Index(data, content)
		if len(content) == 0 || offset < 0 {
			panic("artifact does not embed current file: " + path)
		}
		h := sha256.Sum256(content)
		items = append(items, map[string]any{"file": path, "bytes": len(content), "offset": offset, "sha256": hex.EncodeToString(h[:])})
	}
	h := sha256.Sum256(data)
	result := map[string]any{"artifact": *artifact, "bytes": len(data), "sha256": hex.EncodeToString(h[:]), "exact_embedded_files": items,
		"verification_scope":"Byte-identical embedded DLL, Lua and generated Dog scenes. No game runtime test."}
	encoded, err := json.MarshalIndent(result, "", "  ")
	if err != nil {
		panic(err)
	}
	fmt.Println(string(encoded))
}
