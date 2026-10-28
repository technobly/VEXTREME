# config/

`profile.mk` (optional, not in the public tree) selects build options without editing shared files:

```
VXE_LAYOUT ?= dfu       # uf2 (default) or dfu (use ?= so a command-line choice still wins, also inside docker)
# KERNEL_OPT := -Os      # default follows the layout
```

A private build adds this one file. Because the public tree has no `config/profile.mk`, pulling the public
repo into a private one never conflicts on it.
