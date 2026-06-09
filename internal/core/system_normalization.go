package core

import (
	"regexp"
	"strconv"
	"strings"
)

var versionMajorMinorPattern = regexp.MustCompile(`(?i)(\d+)\.(\d+)`)

// SystemInfoValues is the common raw shape used to normalize local and ProtonDB system profiles.
type SystemInfoValues struct {
	GPUVendor   string
	GPUModel    string
	GPUDriver   string
	CPU         string
	RAMGB       float64
	Distro      string
	Kernel      string
	SessionType string
	Desktop     string
	Raw         map[string]string
}

// NormalizeSystemProfile converts a detected local profile into comparable coarse categories.
func NormalizeSystemProfile(profile SystemProfile) NormalizedSystemProfile {
	return NormalizeSystemInfoValues(SystemInfoValues{
		GPUVendor:   profile.GPUVendor,
		GPUModel:    profile.GPUModel,
		GPUDriver:   profile.GPUDriver,
		CPU:         profile.CPU,
		RAMGB:       profile.RAMGB,
		Distro:      profile.Distro,
		Kernel:      profile.Kernel,
		SessionType: profile.SessionType,
		Desktop:     profile.Desktop,
		Raw:         profile.Raw,
	})
}

// NormalizeSystemInfoMap normalizes ProtonDB-style string maps into the same categories as local profiles.
func NormalizeSystemInfoMap(fields map[string]string) NormalizedSystemProfile {
	return NormalizeSystemInfoValues(SystemInfoValues{
		GPUVendor:   firstMapString(fields, "gpuVendor", "gpu_vendor", "vendor"),
		GPUModel:    firstMapString(fields, "gpuModel", "gpu_model", "gpuName", "gpu_name", "gpu", "graphics", "videoCard", "video_card", "name", "model"),
		GPUDriver:   firstMapString(fields, "gpuDriver", "gpu_driver", "driver", "driverVersion", "driver_version"),
		CPU:         firstMapString(fields, "cpu", "cpuModel", "cpu_model", "processor"),
		RAMGB:       parseRAMGB(firstMapString(fields, "ramGb", "ramGB", "ram_gb", "ram", "memory")),
		Distro:      firstMapString(fields, "distro", "distribution", "os", "osName", "os_name"),
		Kernel:      firstMapString(fields, "kernel", "kernelVersion", "kernel_version"),
		SessionType: firstMapString(fields, "sessionType", "session_type", "session"),
		Desktop:     firstMapString(fields, "desktop", "desktopEnvironment", "desktop_environment"),
		Raw:         fields,
	})
}

// NormalizeSystemInfoValues converts raw system fields into intentionally coarse comparison categories.
func NormalizeSystemInfoValues(values SystemInfoValues) NormalizedSystemProfile {
	distroValues := []string{
		firstMapString(values.Raw, "os-release.ID", "ID"),
		firstMapString(values.Raw, "os-release.ID_LIKE", "ID_LIKE"),
		values.Distro,
	}

	return NormalizedSystemProfile{
		GPUVendor:    normalizeGPUVendor(values.GPUVendor + " " + values.GPUModel),
		GPUModel:     simplifyGPUModel(values.GPUModel),
		GPUDriver:    versionMajorMinor(values.GPUDriver),
		CPUVendor:    normalizeCPUVendor(values.CPU),
		CPUClass:     normalizeCPUClass(values.CPU),
		RAMBucket:    ramBucket(values.RAMGB),
		DistroFamily: normalizeDistroFamily(distroValues...),
		Kernel:       versionMajorMinor(values.Kernel),
		SessionType:  normalizeSessionType(firstNonEmptySystemString(values.SessionType, values.Desktop)),
	}
}

func normalizeGPUVendor(value string) string {
	lower := strings.ToLower(value)
	switch {
	case strings.Contains(lower, "nvidia") || strings.Contains(lower, "geforce") || strings.Contains(lower, "rtx") || strings.Contains(lower, "gtx"):
		return "nvidia"
	case strings.Contains(lower, "advanced micro devices") || strings.Contains(lower, "amd/ati") || strings.Contains(lower, "amd") || strings.Contains(lower, "radeon") || strings.Contains(lower, "radv"):
		return "amd"
	case strings.Contains(lower, "intel") || strings.Contains(lower, "arc") || strings.Contains(lower, "iris") || strings.Contains(lower, "uhd graphics"):
		return "intel"
	default:
		return "unknown"
	}
}

func simplifyGPUModel(value string) string {
	value = strings.TrimSpace(value)
	if value == "" {
		return ""
	}
	if start := strings.Index(value, "["); start >= 0 {
		if end := strings.Index(value[start+1:], "]"); end >= 0 {
			value = value[start+1 : start+1+end]
		}
	}
	if idx := strings.Index(value, "("); idx >= 0 {
		value = strings.TrimSpace(value[:idx])
	}
	lower := strings.ToLower(value)
	replacers := []string{
		"nvidia corporation", "nvidia", "advanced micro devices, inc.", "advanced micro devices", "amd/ati", "amd", "ati technologies inc.", "intel corporation", "intel",
	}
	for _, old := range replacers {
		lower = strings.ReplaceAll(lower, old, " ")
	}
	lower = strings.NewReplacer("[", " ", "]", " ", ",", " ").Replace(lower)
	return strings.Join(strings.Fields(lower), " ")
}

func normalizeCPUVendor(value string) string {
	lower := strings.ToLower(value)
	switch {
	case strings.Contains(lower, "amd") || strings.Contains(lower, "ryzen") || strings.Contains(lower, "epyc"):
		return "amd"
	case strings.Contains(lower, "intel") || strings.Contains(lower, "core(tm)") || strings.Contains(lower, "core i") || strings.Contains(lower, "xeon"):
		return "intel"
	case strings.Contains(lower, "apple"):
		return "apple"
	default:
		return "unknown"
	}
}

func normalizeCPUClass(value string) string {
	lower := strings.ToLower(value)
	switch {
	case strings.Contains(lower, "ryzen 9"):
		return "ryzen 9"
	case strings.Contains(lower, "ryzen 7"):
		return "ryzen 7"
	case strings.Contains(lower, "ryzen 5"):
		return "ryzen 5"
	case strings.Contains(lower, "ryzen 3"):
		return "ryzen 3"
	case strings.Contains(lower, "threadripper"):
		return "threadripper"
	case strings.Contains(lower, "epyc"):
		return "epyc"
	case strings.Contains(lower, "core ultra"):
		return "core ultra"
	case strings.Contains(lower, "core i9") || strings.Contains(lower, "i9-"):
		return "core i9"
	case strings.Contains(lower, "core i7") || strings.Contains(lower, "i7-"):
		return "core i7"
	case strings.Contains(lower, "core i5") || strings.Contains(lower, "i5-"):
		return "core i5"
	case strings.Contains(lower, "core i3") || strings.Contains(lower, "i3-"):
		return "core i3"
	case strings.Contains(lower, "xeon"):
		return "xeon"
	default:
		return "unknown"
	}
}

func ramBucket(ramGB float64) string {
	switch {
	case ramGB <= 0:
		return "unknown"
	case ramGB < 8:
		return "<8"
	case ramGB < 16:
		return "8-15"
	case ramGB < 32:
		return "16-31"
	default:
		return "32+"
	}
}

func normalizeDistroFamily(values ...string) string {
	lower := strings.ToLower(strings.Join(values, " "))
	switch {
	case strings.Contains(lower, "arch") || strings.Contains(lower, "manjaro") || strings.Contains(lower, "endeavour") || strings.Contains(lower, "steamos"):
		return "arch"
	case strings.Contains(lower, "fedora") || strings.Contains(lower, "nobara") || strings.Contains(lower, "bazzite") || strings.Contains(lower, "ublue"):
		return "fedora"
	case strings.Contains(lower, "ubuntu") || strings.Contains(lower, "pop!_os") || strings.Contains(lower, "pop os") || strings.Contains(lower, "mint") || strings.Contains(lower, "neon"):
		return "ubuntu"
	case strings.Contains(lower, "debian"):
		return "debian"
	default:
		return "unknown"
	}
}

func normalizeSessionType(value string) string {
	lower := strings.ToLower(value)
	switch {
	case strings.Contains(lower, "wayland"):
		return "wayland"
	case strings.Contains(lower, "x11") || strings.Contains(lower, "xorg"):
		return "x11"
	default:
		return "unknown"
	}
}

func versionMajorMinor(value string) string {
	matches := versionMajorMinorPattern.FindStringSubmatch(value)
	if matches == nil {
		return ""
	}
	return matches[1] + "." + matches[2]
}

func parseRAMGB(value string) float64 {
	value = strings.TrimSpace(strings.ToLower(value))
	if value == "" {
		return 0
	}
	unit := "gb"
	switch {
	case strings.Contains(value, "mib") || strings.Contains(value, "mb"):
		unit = "mb"
	case strings.Contains(value, "kib") || strings.Contains(value, "kb"):
		unit = "kb"
	}
	fields := regexp.MustCompile(`[-+]?\d+(?:\.\d+)?`).FindString(value)
	if fields == "" {
		return 0
	}
	parsed, err := strconv.ParseFloat(fields, 64)
	if err != nil {
		return 0
	}
	switch unit {
	case "mb":
		return parsed / 1024
	case "kb":
		return parsed / 1024 / 1024
	default:
		return parsed
	}
}

func firstMapString(fields map[string]string, keys ...string) string {
	if fields == nil {
		return ""
	}
	for _, key := range keys {
		if value := strings.TrimSpace(fields[key]); value != "" {
			return value
		}
	}
	for _, key := range keys {
		for actualKey, value := range fields {
			if strings.EqualFold(actualKey, key) && strings.TrimSpace(value) != "" {
				return strings.TrimSpace(value)
			}
		}
	}
	return ""
}

func firstNonEmptySystemString(values ...string) string {
	for _, value := range values {
		if strings.TrimSpace(value) != "" {
			return strings.TrimSpace(value)
		}
	}
	return ""
}
