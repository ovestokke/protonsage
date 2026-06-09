package steam

import "testing"

func TestParseLocalConfigLaunchOptions(t *testing.T) {
	options, err := ParseLocalConfigLaunchOptionsFile("../../testdata/steam/localconfig.vdf")
	if err != nil {
		t.Fatal(err)
	}
	if options[123] != "PROTON_USE_WINED3D=1 %command%" {
		t.Fatalf("options[123] = %q", options[123])
	}
	if options[456] != "gamemoderun %command%" {
		t.Fatalf("options[456] = %q", options[456])
	}
}

func TestParseLocalConfigLaunchOptionsFallback(t *testing.T) {
	obj, err := ParseVDF([]byte(`"root" { "apps" { "789" { "launchoptions" "mangohud %command%" } } }`))
	if err != nil {
		t.Fatal(err)
	}
	options := ParseLocalConfigLaunchOptions(obj)
	if options[789] != "mangohud %command%" {
		t.Fatalf("options[789] = %q", options[789])
	}
}
