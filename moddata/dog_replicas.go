package moddata

import (
	"fmt"
	"path/filepath"
)

func init() {
	for variant := 0; variant < 5; variant++ {
		name := fmt.Sprintf("buddy_dog2_%02d", variant)
		ReplaceInFPK = append(ReplaceInFPK, UpdateInFpk{
			Source:  FileFromQar{SourceQar: filepath.Join("master", "chunk1.dat"), NameInQar: "/Assets/tpp/pack/buddy/dog/" + name + ".fpkd"},
			Replace: []FileToAdd{{SourceFile: "dog_replicas/" + name + ".fox2", FileName: "/Assets/tpp/level_asset/chara/buddy/dog/" + name + ".fox2"}},
		})
	}
}
