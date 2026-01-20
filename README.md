# pbgtk

Packfile extraction and packing tool for PBG formats, used by the Seihou shoot-'em-up series and other PBG-developed games like Samidare

Compatible with Windows 95-XP (pbgtkret) and Windows Vista-11 (pbgtk), with Linux support being considered

To use properly on Windows 9x, run the [Microsoft Layer for Unicode Redistributable](https://web.archive.org/web/20041210120000id_/https://download.microsoft.com/download/b/7/5/b75eace3-00e2-4aa0-9a6f-0b6882c71642/unicows.exe) and browse to the folder where pbgtk is for extraction, when prompted.

## PBG Formats

| Format        | Extraction | Packing | Games                                |
| ------------- | ---------- | ------- | ------------------------------------ |
| PBG1A (.dat)  | Yes        | Yes     | Seihou 1 (Shuusou Gyoku)             |
| PBG3 (.dat)   | Yes        | Yes     | Seihou 2 (Kioh Gyoku)                |
| PBG4 (.dat)   | Yes        | Yes     | Seihou 3 (Banshiryuu, pre-C67)       |
| PBG5 (.ac5)   | Yes        | Yes     | Seihou 3 (Banshiryuu, C67), Samidare |
| PBG6 (.ac6)   | Yes        | Yes     | Seihou 3 (Banshiryuu, C74)           |
| AC7 (.ac7)    | Planned    | Planned | Early SUPER Mate Mate Laser          |
| AC8 (.ac8)    | Planned    | Planned | Later SUPER Mate Mate Laser          |

## Usage

Usage: ```pbgtk extract version in_dat out_folder (--rename (preset))```

OR     ```pbgtk pack version in_folder out_dat (--remove-extensions)```

Version can be:

`1` - PBG1A

`3` - PBG3

`4` - PBG4

`5` - PBG5

`6` - PBG6

PBG1A packfiles (in Seihou 1) do not have filenames, and PBG3 packfiles (in Seihou 2) do not have file extensions. Thus, the `--rename` option is available to give these files more intuitive names.

Auto-renaming presets:

For Seihou 1 (PBG1A): `enemy`, `graph`, `graph2`, `music`, or `sound`

For Seihou 2 (PBG3): `enemy`, `graph`, `graph2`, `graph3`, `music`, or `sound`

(`--remove-extensions` must be used when packing if `--rename` was used to extract a Seihou 2 packfile!)

Examples:
- `pbgtk extract 5 Grp.ac5 Grp` (extracts all files from packfile Grp.ac5 to folder Grp)
- `pbgtk pack 5 Grp Grp_repack.ac5` (packs all files from folder Grp to packfile Grp_repack.ac5)
- `pbgtk extract 1 GRAPH.DAT GRAPH --rename graph` (extracts all files from packfile GRAPH.DAT to folder GRAPH, automatically giving them meaningful filenames according to the Seihou 1 GRAPH.DAT preset)
- `pbgtk extract 3 GRAPH2.DAT GRAPH2 --rename graph2` (extracts all files from packfile GRAPH2.DAT to folder GRAPH2, automatically giving them meaningful filenames according to the Seihou 2 GRAPH2.DAT preset)
- `pbgtk pack 3 GRAPH2 GRAPH2_repack.DAT --remove-extensions` (packs all files from folder GRAPH2 to packfile GRAPH2_repack.DAT, removing file extensions as required by Seihou 2)

Graphics can be modified using a preferred photo editor (just make sure it supports indexed-color bitmaps properly in the case of Seihou 1 and some of 2). To modify stage and dialogue scripts (ECL, SCL, etc.), use [SSGtk](https://github.com/Clb184/SSGtk), [KOGtk](https://github.com/Clb184/KOGtk), [BSRtk_C67](https://github.com/Clb184/BSRtk_C67), or [BSRtk](https://github.com/Clb184/BSRtk), depending on the game. Tools are in the works for modifying Samidare scripts at the moment.

Have fun modding/translating! :)

## Special Thanks

- [nmlgc](https://github.com/nmlgc) (C++ LZSS and range coder implementation; documentation of PBG1A and PBG6)
- [thtk Developers](https://github.com/thpatch/thtk) (Optimized LZSS compression; documentation of PBG3 and PBG4)
- [PyTouhou Developers](https://github.com/GovanifY/PyTouhou/blob/master/doc/PBG3) (Further documentation of PBG3 and its bitstreams)
- [ExpHP](https://github.com/ExpHP) (Documentation of PBG5)
- [Clb184](https://github.com/Clb184) (Making tools for modifying stage and dialogue scripts from Seihou games)