package steam

import (
	"os"
	"testing"
)

func TestParseVDFLibraryFolders(t *testing.T) {
	data, err := os.ReadFile("../../testdata/steam/libraryfolders.vdf")
	if err != nil {
		t.Fatal(err)
	}
	obj, err := ParseVDF(data)
	if err != nil {
		t.Fatal(err)
	}
	folders, ok := AsObject(obj["libraryfolders"])
	if !ok {
		t.Fatal("missing libraryfolders object")
	}
	entry, ok := AsObject(folders["1"])
	if !ok {
		t.Fatal("missing second library entry")
	}
	if got := StringValue(entry, "path"); got != "/mnt/games/SteamLibrary" {
		t.Fatalf("path = %q", got)
	}
}

func TestParseVDFCommentsAndEscapes(t *testing.T) {
	obj, err := ParseVDF([]byte("// comment\n\"root\" { \"name\" \"A \\\"quoted\\\" Game\" }"))
	if err != nil {
		t.Fatal(err)
	}
	root, ok := AsObject(obj["root"])
	if !ok {
		t.Fatal("missing root")
	}
	if got := StringValue(root, "name"); got != "A \"quoted\" Game" {
		t.Fatalf("name = %q", got)
	}
}
