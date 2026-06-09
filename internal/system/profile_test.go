package system

import (
	"testing"

	"protonsage/internal/core"
)

func TestParseOSRelease(t *testing.T) {
	fields := ParseOSRelease("NAME=\"Nobara Linux\"\nVERSION_ID=40\nPRETTY_NAME=\"Nobara Linux 40\"\n")
	if fields["PRETTY_NAME"] != "Nobara Linux 40" {
		t.Fatalf("PRETTY_NAME = %q", fields["PRETTY_NAME"])
	}
	if got := distroName(fields); got != "Nobara Linux 40" {
		t.Fatalf("distro = %q", got)
	}
}

func TestParseCPUInfo(t *testing.T) {
	cpu := ParseCPUInfo("processor\t: 0\nmodel name\t: AMD Ryzen 7 7800X3D 8-Core Processor\n")
	if cpu != "AMD Ryzen 7 7800X3D 8-Core Processor" {
		t.Fatalf("cpu = %q", cpu)
	}
}

func TestParseMemInfoGB(t *testing.T) {
	gb := ParseMemInfoGB("MemTotal:       32768000 kB\n")
	if gb != 31.3 {
		t.Fatalf("gb = %.1f", gb)
	}
}

func TestParseLspciGPU(t *testing.T) {
	vendor, model := ParseLspciGPU("01:00.0 VGA compatible controller: NVIDIA Corporation AD104 [GeForce RTX 4070 SUPER] (rev a1)\n")
	if vendor != "NVIDIA" {
		t.Fatalf("vendor = %q", vendor)
	}
	if model != "NVIDIA Corporation AD104 [GeForce RTX 4070 SUPER] (rev a1)" {
		t.Fatalf("model = %q", model)
	}
}

func TestProfileNormalizationUsesParsedFields(t *testing.T) {
	profile := core.SystemProfile{
		GPUVendor: "AMD",
		GPUModel:  "Radeon RX 7800 XT",
		Distro:    "Ubuntu 24.04",
		Raw: map[string]string{
			"os-release.ID": "ubuntu",
		},
	}
	profile.Normalized = core.NormalizeSystemProfile(profile)
	if profile.Normalized.GPUVendor != "amd" {
		t.Fatalf("normalized GPU vendor = %q", profile.Normalized.GPUVendor)
	}
	if profile.Normalized.DistroFamily != "ubuntu" {
		t.Fatalf("normalized distro family = %q", profile.Normalized.DistroFamily)
	}
}
