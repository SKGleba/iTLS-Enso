# iTLS-Enso
Adds TLS v1.2 to devices with enso installed.

# How-To 

1) Install the provided VPK file.

2) Start the app, select "Install the full iTLS package" and wait til it finishes.
   - You may want to only install the new root certs, to do it select "Install the iTLS CA certs"

3) Reboot.

# Credits

Thanks to:
- Team Molecule
- Testers from the HenKaku discord server

# FAQ

- Can I use it without enso?
  - Yes for browser-only patch, but it is strongly recommended to use enso.
- Where can I find the CA certs?
  - vs0:/data/external/cert/CA_LIST.cer
- How can I update the tool?
  - Uninstall via the previous installer, install via the new one.

# Notes
- Since version 3.0 iTLS also patches apps-side TLS stuff.
- Updating CA certs may fix some problems with web sites, apps (like netflix), etc.
- For any help/serious issue DM me on twitter (twitter.com/skgleba)

## CURRENT MODULES ORIGIN FW: 3.73
