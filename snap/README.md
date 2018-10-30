[![Snap Status](https://build.snapcraft.io/badge/reicast/reicast-emulator.svg)](https://build.snapcraft.io/user/reicast/reicast-emulator)

# General Notes
---
Snap is a **squashfs** used to distribute applications on Linux targets in a
**secure**, **easy** and **maintainable** fashion.

[Snap commands official reference](https://docs.snapcraft.io/reference/snap-command)

# Installation
---
### Installing snap 
- Info on installing snap can be found at: [The official snap installation guide](https://docs.snapcraft.io/core/install)
- For more info check: [Complete snap usage article](https://itsfoss.com/use-snap-packages-ubuntu-16-04/)

- Most Linux distros are covered there. If you encounter a problem contact us @
  lx0@emudev.org.

### Installing reicast
- To install **reicast** (after having snap setup), simply run:

```bash
snap install reicast --edge 
```
- This will get the latest master build for **reicast**.
- _If it fails_, run it with **sudo**.

# Developer Notes
---
- snap and snapcraft info:
	* Snapcraft is a command line tool used to build snaps. 
	* For now, all snaps should be built to run against the ‘Series 16’ core.
	* Snaps are built to run against a base snap containing a minimal common runtime environment.
	* The best experience is generally to use a clean Ubuntu 16.04.3 LTS system or LXD containers or VM running Ubuntu 16.04.3.
	* Snapcraft is itself available as a snap.
	* Snapcraft builds on top of tools like autotools, make, and cmake to create snaps for people to install on Linux.

- Additional Links:
	* [The snapcraft syntax](https://docs.snapcraft.io/build-snaps/syntax#parts)
	* [Snapcraft commands reference](https://docs.snapcraft.io/reference/snapcraft-command)
	* [Snapcraft plugins reference](https://docs.snapcraft.io/reference/plugins/)
