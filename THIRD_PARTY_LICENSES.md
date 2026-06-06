# Third-Party Licenses & Credits

Akashic Vault is licensed under **GPL-3.0** (see `LICENSE`). It incorporates
third-party DSP cores and libraries, each under its own license. Every ported or
copied component must be listed here with its upstream source, author(s), and
license, and must keep its original license header in the source tree.

> Add a row whenever a module or library is vendored. Confirm GPL-3.0 compatibility
> before merging.

## Platform / libraries

| Component | Source | Author | License |
|-----------|--------|--------|---------|
| circle-stdlib | https://github.com/smuehlst/circle-stdlib | Stephan Mühlstrasser | GPL-3.0 |
| Circle (libs/circle) | https://github.com/Akashic-Trance-Machines/circle (fork of rsta2/circle) | Rene Stange | GPL-3.0 |
| Circle `addon/display` SSD1309 driver | ATM circle fork, `feature/ssd1309-display` | R. Stange (ATM) | GPL-3.0 |
| Circle `addon/gpio` MCP23017 driver | ATM circle fork, `feature/mcp23017-driver` | T. Nelissen | GPL-3.0 |
| LVGL | https://github.com/lvgl/lvgl (Circle `addon/lvgl`) | LVGL Kft | MIT |
| CMSIS-5 (DSP) | https://github.com/ARM-software/CMSIS_5 | ARM | Apache-2.0 |

## Modules (sound generators / audio FX / MIDI FX)

| Module | Core source | Original author | Port | License |
|--------|-------------|-----------------|------|---------|
| _(example)_ Dexed | https://github.com/asb2m10/dexed (MSFA) | Google / asb2m10 | charlesvestal / ATM | GPL-3.0 |
| _(example)_ CloudSeed | Ghost Note Audio | Valdemar Erlingsson | charlesvestal / ATM | MIT |

_Replace the example rows as real modules are added. Each module repo must also
contain its own `LICENSE` and `CREDITS.md`._
