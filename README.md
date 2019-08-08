# iTLS-Enso
Adds TLS v1.2 to Enso enabled devices.

# How-To (perma version - requires enso)

1) Install the provided VPK file.

2) Start the app, select "Install 3.68 compat module" and wait til it finishes.
   - You may want to update CA certs, to do it open the app again and select "Install 3.69 root certs"

3) Reboot.

# How-To (plugin version - requires ioplus v0.1 or higher)

0) Make sure that you have ioplus by dots_tb installed (https://github.com/CelesteBlue-dev/PSVita-RE-tools/tree/master/ioPlus)

1) Install the provided VPK file.

2) In tai config.txt add "ur0:itls/itls.suprx" under \*ALL.

3) Start the app, select "Install/Uninstall the iTLS plugin" and wait til it finishes.
   - You may want to update CA certs, to do it open the app again and select "Install 3.69 root certs"

4) Reboot.

# Build .vpk

1) Run `./build.sh build`
2) To clean the built files, run: `./build.sh clean`

# Credits

Thanks to:
- dotsie.
- Celeste.
- @TakuyaKou.
- noname120.
- TheFlow
- Beta testers from Vita Hacking and VitaMods discord servers.

# FAQ

- Can i use it without enso?
  - Yes, but you must enable homebrew before using the browser.
- Where can I find the CA certs?
  - vs0:/data/external/cert/CA_LIST.cer

# Notes

- Updating CA certs may fix some problems with web sites, apps (like netflix), etc.
- For any help/serious issue DM me on twitter (twitter.com/skgleba)
