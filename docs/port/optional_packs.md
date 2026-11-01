# Optional installation packs

Setup offers Spanish, English and Brazilian Portuguese. The Portuguese option
uses the English BT4 B14 Rev2 disc profile plus the supplied `BT4_PT-BR_v1.2.zip`
translation. An English base can be extracted from an English ISO or downloaded
through the existing language-package mechanism after installing the Spanish ISO.

Portuguese resources are installed under `PTBR/`. Game settings and the native
startup language card select the profile; no command script, PowerShell process,
directory junction or separate game executable is needed. Voice and music retain
the English base profile. Translation coverage is that of the supplied v1.2 pack;
this integration does not add missing translations to that archive.

The native file resolver checks the Portuguese presentation overrides before
the selected base files. The renderer loads Portuguese pictures before the HD
pack and uses the package's font-conflict list. Spanish and English do not load
the isolated Portuguese texture directory. Switching full game languages takes
effect at the next launch, as for the other disc profiles.

The optional **DBZ BT4 HD v1.07 (3.69 GB)** pack remains compressed in `textures/`
and is read on demand. GitHub's per-asset size restriction requires two download
parts; setup joins them and verifies the complete ZIP with SHA-256. Local ZIP
selection remains available for both packs. The installer shows download and
installation progress, removes successful download caches, and backs up files
that it replaces. Reinstallation compares files and repairs missing entries.

R9's package downloads are attached to the R9 release in this repository. Draft
assets require repository access until the release is published. Credentials are
obtained through the existing GitHub CLI authorization flow and are never embedded
in setup or the game.
