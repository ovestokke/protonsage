package core

import "testing"

func TestNormalizeSystemProfile(t *testing.T) {
	profile := SystemProfile{
		GPUVendor:   "NVIDIA",
		GPUModel:    "NVIDIA Corporation AD104 [GeForce RTX 4070 SUPER] (rev a1)",
		GPUDriver:   "535.154.05",
		CPU:         "AMD Ryzen 7 7800X3D 8-Core Processor",
		RAMGB:       31.3,
		Distro:      "EndeavourOS",
		Kernel:      "6.14.4-arch1-1",
		SessionType: "wayland",
		Raw: map[string]string{
			"os-release.ID":      "endeavouros",
			"os-release.ID_LIKE": "arch",
		},
	}

	normalized := NormalizeSystemProfile(profile)
	if normalized.GPUVendor != "nvidia" {
		t.Fatalf("GPUVendor = %q", normalized.GPUVendor)
	}
	if normalized.GPUModel != "geforce rtx 4070 super" {
		t.Fatalf("GPUModel = %q", normalized.GPUModel)
	}
	if normalized.GPUDriver != "535.154" {
		t.Fatalf("GPUDriver = %q", normalized.GPUDriver)
	}
	if normalized.CPUVendor != "amd" || normalized.CPUClass != "ryzen 7" {
		t.Fatalf("CPU normalization = %+v", normalized)
	}
	if normalized.RAMBucket != "16-31" {
		t.Fatalf("RAMBucket = %q", normalized.RAMBucket)
	}
	if normalized.DistroFamily != "arch" || normalized.Kernel != "6.14" || normalized.SessionType != "wayland" {
		t.Fatalf("unexpected normalized profile: %+v", normalized)
	}
}

func TestNormalizeSystemInfoMap(t *testing.T) {
	normalized := NormalizeSystemInfoMap(map[string]string{
		"gpu":           "AMD Radeon RX 7800 XT",
		"driverVersion": "Mesa 25.1.2",
		"cpu":           "Intel Core i5-12600K",
		"ram":           "32 GB",
		"os":            "Nobara Linux 40",
		"kernel":        "6.8.12-200.fc40.x86_64",
		"sessionType":   "x11",
	})
	if normalized.GPUVendor != "amd" || normalized.GPUModel != "radeon rx 7800 xt" {
		t.Fatalf("GPU normalization = %+v", normalized)
	}
	if normalized.GPUDriver != "25.1" {
		t.Fatalf("GPUDriver = %q", normalized.GPUDriver)
	}
	if normalized.CPUVendor != "intel" || normalized.CPUClass != "core i5" {
		t.Fatalf("CPU normalization = %+v", normalized)
	}
	if normalized.RAMBucket != "32+" || normalized.DistroFamily != "fedora" || normalized.Kernel != "6.8" || normalized.SessionType != "x11" {
		t.Fatalf("unexpected normalized report system info: %+v", normalized)
	}
}
