# PuTTYTray

This branch contains an attempt to re-ignite PuTTYTray on PuTTY 0.68 or higher.

### Project status

The project has fallen behind PuTTY upstream, and shouldn't be used for now.
**Please use [upstream PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html)**.

There is discussion of the [project status on github issues](https://github.com/FauxFaux/PuTTYTray/issues/278).

The previously released binaries are built from the [latest PuTTYTray tag](https://github.com/FauxFaux/PuTTYTray/tree/p0.67-t029).

### Branches:

 * `upstream`: the actual upstream branch we're tracking (master)
 * `upclean`: an automatically cleaned-up variant of the branch
 * `scripts`: orphan branch carrying the scripts to generate upclean
 * `master`: third attempt to reignite the project
 * `t67-failed-merge`: a failed attempt to merge `0.68` and `p0.67-t029`
 * `t67-failed-patches`: a failed attempt to make patches from `p0.67-t029`

I will rebase all branches. Please work from tags.

**All branches are a mess**. Do not use them. Do not look at them.
Do not raise issues or pull requests about them.


### Projects:

 * `proj`: miscellaneous changes necessary to run a project (e.g. README)
 * `cmake`: build system extensions to use cmake, could be merged
 * `url`: support for clickable urls, url menu, etc.
 * `backport`: pulling changes back from the next release
 * `zoom`: increase font size with the mousewheel
 * `fatty`: bundle `agent` and `gen` into `putty.exe`
 * `icon`: tray icon and extra menu / actions
 * `import`: transparent support for openssh key formats


### cmake

```bash
mkdir -p doc && \
perl licence.pl && \
(cd charset && perl sbcsgen.pl) && \
./mkcmake.py > CMakeLists.txt
```
