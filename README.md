# iTLS-Enso
Adds TLS v1.2 to devices with enso installed.

# How-To 
1) Install the provided VPK file.

2) Start the app, select "Install the full iTLS package" and wait until it finishes.
   - You may want to install only the new root certs, to do it select "Install the iTLS CA certs".

3) Reboot.

# FAQ
- Can I use it without enso?
  - Yes for the browser patch only.
- Where can I find the CA certs?
  - vs0:/data/external/cert/CA_LIST.cer
- Can I delete the bubble after iTLS is installed?
  - Yes, the un/installer is independent of the plugin.

# Notes
- Since version 3.0 iTLS also patches app-side TLS stuff.
- Since version 3.2 iTLS also patches the NP server link on firmware 3.60.
- Since version 3.2.1 iTLS bundles external CA certs because SCE didnt update theirs.
- Updating CA certs may fix some problems with web sites, apps (like Netflix), etc.
- For any help/serious issue DM me on [Twitter](twitter.com/skgleba).

# Credits
Thanks to:
- Team Molecule
- Testers from the HenKaku Discord server
- Cuevavirus for the 3.60 NP server fix
- CreepNT for the vshIoMount fix
- olokos for the new external cert bundle

## CURRENT MODULES ORIGIN FW: 3.73
## CURRENT CAs ORIGIN FW: 3.74 + external
