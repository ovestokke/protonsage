package system

import (
	"bufio"
	"bytes"
	"context"
	"math"
	"os"
	"os/exec"
	"runtime"
	"strconv"
	"strings"
	"time"

	"protonsage/internal/core"
)

// DetectProfile performs read-only best-effort local system detection.
func DetectProfile() core.SystemProfile {
	profile := core.SystemProfile{Raw: map[string]string{}}

	if data, err := os.ReadFile("/etc/os-release"); err == nil {
		fields := ParseOSRelease(string(data))
		profile.Distro = distroName(fields)
		for key, value := range fields {
			profile.Raw["os-release."+key] = value
		}
	}

	if kernel := runOutput(2*time.Second, "uname", "-r"); kernel != "" {
		profile.Kernel = kernel
	} else {
		profile.Kernel = runtime.GOOS
	}

	if data, err := os.ReadFile("/proc/cpuinfo"); err == nil {
		profile.CPU = ParseCPUInfo(string(data))
	}
	if data, err := os.ReadFile("/proc/meminfo"); err == nil {
		profile.RAMGB = ParseMemInfoGB(string(data))
	}

	if lspci := runOutput(2*time.Second, "lspci"); lspci != "" {
		profile.GPUVendor, profile.GPUModel = ParseLspciGPU(lspci)
	}
	if profile.GPUDriver == "" && strings.EqualFold(profile.GPUVendor, "NVIDIA") {
		profile.GPUDriver = runOutput(2*time.Second, "nvidia-smi", "--query-gpu=driver_version", "--format=csv,noheader")
	}

	profile.SessionType = os.Getenv("XDG_SESSION_TYPE")
	profile.Desktop = firstNonEmpty(os.Getenv("XDG_CURRENT_DESKTOP"), os.Getenv("DESKTOP_SESSION"))

	if profile.Raw == nil {
		profile.Raw = map[string]string{}
	}
	profile.Raw["goos"] = runtime.GOOS
	profile.Raw["goarch"] = runtime.GOARCH
	profile.Normalized = core.NormalizeSystemProfile(profile)
	return profile
}

// ParseOSRelease parses /etc/os-release style key/value data.
func ParseOSRelease(data string) map[string]string {
	fields := map[string]string{}
	scanner := bufio.NewScanner(strings.NewReader(data))
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, "#") || !strings.Contains(line, "=") {
			continue
		}
		parts := strings.SplitN(line, "=", 2)
		key := strings.TrimSpace(parts[0])
		value := strings.TrimSpace(parts[1])
		value = strings.Trim(value, "\"")
		value = strings.ReplaceAll(value, `\"`, `"`)
		fields[key] = value
	}
	return fields
}

// ParseCPUInfo returns the first useful CPU model string from /proc/cpuinfo.
func ParseCPUInfo(data string) string {
	scanner := bufio.NewScanner(strings.NewReader(data))
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "model name") || strings.HasPrefix(line, "Hardware") {
			parts := strings.SplitN(line, ":", 2)
			if len(parts) == 2 {
				return strings.TrimSpace(parts[1])
			}
		}
	}
	return ""
}

// ParseMemInfoGB returns MemTotal from /proc/meminfo as GiB rounded to one decimal.
func ParseMemInfoGB(data string) float64 {
	scanner := bufio.NewScanner(strings.NewReader(data))
	for scanner.Scan() {
		line := scanner.Text()
		if !strings.HasPrefix(line, "MemTotal:") {
			continue
		}
		fields := strings.Fields(line)
		if len(fields) < 2 {
			return 0
		}
		kb, err := strconv.ParseFloat(fields[1], 64)
		if err != nil {
			return 0
		}
		gb := kb / 1024 / 1024
		return math.Round(gb*10) / 10
	}
	return 0
}

// ParseLspciGPU extracts the first GPU-ish device from lspci output.
func ParseLspciGPU(data string) (vendor string, model string) {
	scanner := bufio.NewScanner(strings.NewReader(data))
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		lower := strings.ToLower(line)
		if !strings.Contains(lower, "vga compatible controller") && !strings.Contains(lower, "3d controller") && !strings.Contains(lower, "display controller") {
			continue
		}
		model = line
		if idx := strings.Index(line, ": "); idx >= 0 && idx+2 < len(line) {
			model = line[idx+2:]
		}
		switch {
		case strings.Contains(lower, "nvidia"):
			vendor = "NVIDIA"
		case strings.Contains(lower, "advanced micro devices") || strings.Contains(lower, "amd/ati") || strings.Contains(lower, "radeon"):
			vendor = "AMD"
		case strings.Contains(lower, "intel"):
			vendor = "Intel"
		}
		return vendor, model
	}
	return "", ""
}

func distroName(fields map[string]string) string {
	if pretty := fields["PRETTY_NAME"]; pretty != "" {
		return pretty
	}
	if name := fields["NAME"]; name != "" {
		if version := fields["VERSION_ID"]; version != "" {
			return name + " " + version
		}
		return name
	}
	return ""
}

func firstNonEmpty(values ...string) string {
	for _, value := range values {
		if value != "" {
			return value
		}
	}
	return ""
}

func runOutput(timeout time.Duration, name string, args ...string) string {
	if _, err := exec.LookPath(name); err != nil {
		return ""
	}
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	cmd := exec.CommandContext(ctx, name, args...)
	var stdout bytes.Buffer
	cmd.Stdout = &stdout
	if err := cmd.Run(); err != nil {
		return ""
	}
	return strings.TrimSpace(stdout.String())
}
