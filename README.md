# iTLS-Enso
Adds TLS v1.2 to devices with enso and 0syscall6 installed.

# How-To 

1) Install the provided VPK file.

2) Start the app, select "Install/Uninstall iTLS" and wait til it finishes.
   - You may want to only install/uninstall the new root certs, to do it select "Install/Uninstall the latest CA certs"

3) Reboot.

# Credits

Thanks to:
- Team Molecule
- Testers from the HenKaku discord server

# FAQ

- Can i use it without enso?
  - Yes for browser-only patch, but it is strongly recommended to use enso.
- Can i use it without 0syscall6?
  - You can use decrypted libs, but it is strongly recommended to use 0syscall6.
- Where can I find the CA certs?
  - vs0:/data/external/cert/CA_LIST.cer

# Notes
- Since version 3.0 you need the 0syscall6 patches: https://github.com/SKGleba/0syscall6
- Since version 3.0 iTLS also patches apps-side TLS stuff.
- Updating CA certs may fix some problems with web sites, apps (like netflix), etc.
- For any help/serious issue DM me on twitter (twitter.com/skgleba)

## CURRENT MODULES ORIGIN FW: 3.70
