package steam

import (
	"fmt"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

// LaunchOptionsFromRoot reads userdata/*/config/localconfig.vdf files under a Steam root read-only.
func LaunchOptionsFromRoot(root string) (map[int]string, error) {
	result := map[int]string{}
	pattern := filepath.Join(filepath.Clean(root), "userdata", "*", "config", "localconfig.vdf")
	matches, err := filepath.Glob(pattern)
	if err != nil {
		return nil, err
	}
	sort.Strings(matches)
	for _, path := range matches {
		options, err := ParseLocalConfigLaunchOptionsFile(path)
		if err != nil {
			return result, fmt.Errorf("parse %s: %w", path, err)
		}
		for appid, value := range options {
			if _, exists := result[appid]; exists {
				continue
			}
			result[appid] = value
		}
	}
	return result, nil
}

// ParseLocalConfigLaunchOptionsFile parses one localconfig.vdf file read-only.
func ParseLocalConfigLaunchOptionsFile(path string) (map[int]string, error) {
	obj, err := ParseVDFFile(path)
	if err != nil {
		return nil, err
	}
	return ParseLocalConfigLaunchOptions(obj), nil
}

// ParseLocalConfigLaunchOptions extracts per-app LaunchOptions from a parsed Steam localconfig VDF.
func ParseLocalConfigLaunchOptions(obj VDFObject) map[int]string {
	result := map[int]string{}
	if apps, ok := localConfigAppsObject(obj); ok {
		collectLaunchOptionsFromApps(apps, result)
	}
	// Fallback for small fixture variants and future Steam layout changes.
	collectLaunchOptionsRecursive(obj, result)
	return result
}

func localConfigAppsObject(obj VDFObject) (VDFObject, bool) {
	current, ok := objectValueFold(obj, "UserLocalConfigStore")
	if !ok {
		return nil, false
	}
	for _, key := range []string{"Software", "Valve", "Steam", "apps"} {
		current, ok = objectValueFold(current, key)
		if !ok {
			return nil, false
		}
	}
	return current, true
}

func collectLaunchOptionsFromApps(apps VDFObject, result map[int]string) {
	keys := make([]string, 0, len(apps))
	for key := range apps {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	for _, key := range keys {
		appid, err := strconv.Atoi(strings.TrimSpace(key))
		if err != nil || appid <= 0 {
			continue
		}
		entry, ok := AsObject(apps[key])
		if !ok {
			continue
		}
		if value := stringValueFold(entry, "LaunchOptions", "launchoptions", "launchOptions", "Launch Options"); value != "" {
			result[appid] = value
		}
	}
}

func collectLaunchOptionsRecursive(obj VDFObject, result map[int]string) {
	keys := make([]string, 0, len(obj))
	for key := range obj {
		keys = append(keys, key)
	}
	sort.Strings(keys)
	for _, key := range keys {
		child, ok := AsObject(obj[key])
		if !ok {
			continue
		}
		appid, err := strconv.Atoi(strings.TrimSpace(key))
		if err == nil && appid > 0 {
			if value := stringValueFold(child, "LaunchOptions", "launchoptions", "launchOptions", "Launch Options"); value != "" {
				if _, exists := result[appid]; !exists {
					result[appid] = value
				}
			}
		}
		collectLaunchOptionsRecursive(child, result)
	}
}

func objectValueFold(obj VDFObject, key string) (VDFObject, bool) {
	if value, ok := obj[key]; ok {
		return AsObject(value)
	}
	for actualKey, value := range obj {
		if strings.EqualFold(actualKey, key) {
			return AsObject(value)
		}
	}
	return nil, false
}

func stringValueFold(obj VDFObject, keys ...string) string {
	for _, key := range keys {
		if value := strings.TrimSpace(StringValue(obj, key)); value != "" {
			return value
		}
	}
	for _, key := range keys {
		for actualKey, value := range obj {
			if strings.EqualFold(actualKey, key) {
				str, _ := value.(string)
				if strings.TrimSpace(str) != "" {
					return strings.TrimSpace(str)
				}
			}
		}
	}
	return ""
}
