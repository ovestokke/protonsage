export type AppInfo = {
  name: string
  version: string
  stack: string
  capabilities: string[]
}

export type SystemProfile = {
  gpuVendor?: string
  gpuModel?: string
  gpuDriver?: string
  cpu?: string
  ramGb?: number
  distro?: string
  kernel?: string
  sessionType?: string
  desktop?: string
  raw?: Record<string, string>
}

type WailsApp = {
  GetAppInfo?: () => Promise<AppInfo> | AppInfo
  GetSystemProfile?: () => Promise<SystemProfile> | SystemProfile
}

declare global {
  interface Window {
    go?: {
      main?: {
        App?: WailsApp
      }
    }
  }
}

const fallbackInfo: AppInfo = {
  name: 'ProtonSage',
  version: '0.1.0-dev',
  stack: 'Go core + Wails desktop shell + modern React frontend',
  capabilities: [
    'read-only Steam library scan',
    'read-only local system profile',
    'ProtonDB latest snapshot metadata lookup',
    'copy/export first; no Steam config writes',
  ],
}

const fallbackProfile: SystemProfile = {
  gpuVendor: 'Backend offline in browser preview',
  distro: 'Run with Wails to read local system profile',
  kernel: 'preview-mode',
  sessionType: 'web',
  desktop: 'Vite',
}

function backend(): WailsApp | undefined {
  return window.go?.main?.App
}

export async function getAppInfo(): Promise<AppInfo> {
  const app = backend()
  if (app?.GetAppInfo) return await app.GetAppInfo()
  return fallbackInfo
}

export async function getSystemProfile(): Promise<SystemProfile> {
  const app = backend()
  if (app?.GetSystemProfile) return await app.GetSystemProfile()
  return fallbackProfile
}
