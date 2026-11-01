# MasterForge – Auto Mastering für Beat + Vocals

Das Gegenstück zu VocalForge: kommt auf die **Master-Spur**, wenn Beat und Vocals zusammenkommen.
Alles analysiert laufend und passt sich an – du wählst ein Genre-Preset und stellst nur noch nach Gehör nach.

## Signalkette

| # | Modul | Was automatisch passiert | Was du einstellst |
|---|-------|--------------------------|-------------------|
| 1 | **Input & Genre** | – | Gain, Genre (bestimmt die Zielkurve) |
| 2 | **Auto Tonal-EQ** | Misst die Klangbalance in 7 Bändern und gleicht sie an die Genre-Zielkurve an | Stärke, Dunkel/Hell, Bass, Präsenz, Air |
| 3 | **Auto Multiband** | 3 Bänder (Bass / Mitten / Höhen), Thresholds folgen automatisch dem Pegel | Stärke, Tempo |
| 4 | **Glue Comp** | Bus-Kompressor, "klebt" Beat und Vocals zusammen | Stärke, Tempo, Mix |
| 5 | **Sättigung** | – | Drive (Wärme, Dichte) |
| 6 | **Stereo** | – | Breite, Bass Mono (Frequenz), **Vocal Focus** (Stimme in der Mitte nach vorne, Seiten zurück) |
| 7 | **Auto Loudness** | Zieht den Song automatisch auf die Ziel-Lautheit (LUFS), lernt mit, was Clipper/Limiter "kosten" | Ziel LUFS, Clipper, Push (wenn Auto aus), Ceiling |
| 8 | **Output** | Lookahead-Limiter, kein Übersteuern | Trim |

**A/B Original:** spielt den unbearbeiteten Mix **gleich laut** ab – so hörst du ehrlich, ob das Mastering besser klingt, statt nur lauter.

**Presets:** Hip-Hop/Trap (−8 LUFS), Pop (−9), R&B/Soul (−10), Rock (−9), EDM/Club (−7), Lo-Fi (−12),
Akustik/Ballade (−12), Vocal nach vorne, Streaming/Spotify (−14), Sanft – nur polieren.

## Tipps
- Lege MasterForge als **letztes Plugin auf die Master-Spur**. Davor sollte der Mix nicht schon übersteuern (Master-Fader ≤ 0 dB).
- Song einmal **von vorne laufen lassen** – nach wenigen Sekunden hat sich Auto Loudness eingependelt (siehe LUFS-Anzeige).
- Für Spotify/YouTube reichen −14 bis −9 LUFS; lauter (−8/−7) nur, wenn du den "Club-/Trap-Sound" willst.
- Klingt es gequetscht? Clipper und Multiband-Stärke runter oder Ziel-LUFS leiser.
- Rap geht im Beat unter? Preset **"Vocal nach vorne"** oder Vocal Focus hochdrehen.

## Bauen (wie bei VocalForge)
Neues GitHub-Repository anlegen, alle Dateien hochladen, `.github/workflows/build.yml` über
"Add file → Create new file" anlegen, unter **Actions** das Artifact **MasterForge-Windows** herunterladen und
`MasterForge.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.

Lokal: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`
