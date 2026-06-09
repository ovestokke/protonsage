import * as WailsApp from '../wailsjs/wailsjs/go/main/App'
import type * as Models from '../wailsjs/wailsjs/go/models'

// Re-export model types from generated bindings
export type AppInfo = Models.app.AppInfo
export type DataStatus = Models.app.DataStatus
export type InstalledGameStatus = Models.app.InstalledGameStatus
export type SystemProfile = Models.core.SystemProfile
export type NormalizedSystemProfile = Models.core.NormalizedSystemProfile
export type Game = Models.core.Game
export type Recommendation = Models.core.Recommendation
export type Suggestion = Models.core.Suggestion
export type Citation = Models.core.Citation
export type RankedReport = Models.core.RankedReport
export type PreviewResult = Models.core.PreviewResult
export type ImportRun = Models.storage.ImportRun

const isWails = typeof window !== 'undefined' && !!(window as any)?.go?.main?.App

// Wails backend wrappers — dbPath is handled by the Go backend via DbPath().

export async function getAppInfo(): Promise<AppInfo> {
  if (isWails) {
    return WailsApp.GetAppInfo()
  }
  return {
    name: 'ProtonSage',
    version: '0.1.0-dev',
    stack: 'Go core + Wails + React (preview mode)',
    capabilities: [
      'read-only Steam library scan',
      'read-only local system profile',
      'ProtonDB latest snapshot metadata lookup',
      'copy/export first; no Steam config writes',
    ],
  } as AppInfo
}

export async function getSystemProfile(): Promise<SystemProfile> {
  if (isWails) {
    return WailsApp.GetSystemProfile()
  }
  return {
    gpuVendor: 'Preview',
    gpuModel: 'Run with Wails',
    distro: 'browser',
    kernel: 'preview',
    sessionType: 'web',
    normalized: { gpuVendor: 'preview' } as any,
  } as SystemProfile
}

export async function getDataStatus(): Promise<DataStatus> {
  if (isWails) {
    return WailsApp.GetDataStatus()
  }
  return {
    dbPath: '(not configured)',
    sourceCount: 0,
    importRunCount: 0,
    gameCount: 0,
    reportCount: 0,
  } as DataStatus
}

export async function getInstalledGames(): Promise<InstalledGameStatus[]> {
  if (isWails) {
    return WailsApp.GetInstalledGames()
  }
  return []
}

export async function searchGames(query: string, limit?: number): Promise<Game[]> {
  if (isWails) {
    return WailsApp.SearchGames(query, limit || 20)
  }
  return []
}

export async function getRecommendation(appid: number): Promise<Recommendation> {
  if (isWails) {
    return WailsApp.GetRecommendation(appid)
  }
  throw new Error('Recommendations require Wails backend')
}

export async function buildLaunchPreview(
  selected: Suggestion[],
  existing: string,
): Promise<PreviewResult> {
  if (isWails) {
    return WailsApp.BuildLaunchPreview(selected, existing)
  }
  throw new Error('Launch preview requires Wails backend')
}

export async function scanSteam(root?: string): Promise<Game[]> {
  if (isWails) {
    return WailsApp.ScanSteam(root || '')
  }
  return []
}

export async function getDbPath(): Promise<string> {
  if (isWails) {
    return WailsApp.DbPath()
  }
  return ''
}

// Utility: copy text to clipboard
export async function copyToClipboard(text: string): Promise<boolean> {
  try {
    await navigator.clipboard.writeText(text)
    return true
  } catch {
    const textarea = document.createElement('textarea')
    textarea.value = text
    textarea.style.position = 'fixed'
    textarea.style.opacity = '0'
    document.body.appendChild(textarea)
    textarea.select()
    try {
      document.execCommand('copy')
      return true
    } catch {
      return false
    } finally {
      document.body.removeChild(textarea)
    }
  }
}