# ProtonSage UI fixes TODO

## 1. System profile — viser tull  
- [ ] GPU: "?  610...." → må vise "NVIDIA RTX 4070 Ti SUPER · driver 570.x"
- [ ] CPU: mangler → vis "AMD Ryzen 7 9800X3D"
- [ ] RAM: mangler → vis "62 GB"
- [ ] OS: viser ikke distro → vis "CachyOS"
- [ ] DE: viser ikke → vis "niri (Wayland)"
- [ ] Kernel: mangler → vis "Linux 7.0.10"

## 2. Recommendation suggestions — uleselig  
- [ ] Hver suggestion er rå launch-option-tokens, ikke menneskespråk  
- [ ] Må vise: hva det gjør, konfidens, antall rapporter, system-likhet  
- [ ] Eks: "Enable Wayland support" (medium, 12×, sim 55%) i stedet for "PROTON_ENABLE_WAYLAND=1"  

## 3. Launch preview — ubrukelig  
- [ ] Hele notes dumpes som "suggestion" i stedet for ekte env vars  
- [ ] Extraction parser er ødelagt — må fikse så kun %command%-linjer og env-vars ekstraheres  
- [ ] Preview må vise kun valgte env-vars/wrappers, ikke hele rapportteksten  
