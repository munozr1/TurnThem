# TurnThem

A real-time 1v1 battlefield game built with Raylib. Set up artillery, deploy
three-soldier rifle squads as Supplies arrive, and destroy the opposing main
base. The fixed arena has mirrored deployment zones; ground never changes hands.

Working on a feature or fix? Start with the [contribution guide](CONTRIBUTING.md) to choose a branch, check your change, and open a pull request.

## Build Instructions

### Prerequisites
- CMake (version 3.16 or higher; CMake 4.x works)
- C++ compiler with C++17 support
- Git (for fetching ENet, and Raylib if it isn't installed)
- nlohmann-json (client only), e.g. `brew install nlohmann-json`

### Building the Project

1. Clone the repository:
   ```bash
   git clone https://github.com/munozr1/TurnThem.git
   cd TurnThem
   ```

2. Generate build files with CMake:
   ```bash
   cmake -S . -B build
   ```

3. Build everything (game client, server, test bot, tests):
   ```bash
   cmake --build build
   ```

4. Run the tests:
   ```bash
   ctest --test-dir build --output-on-failure
   ```

The build system will automatically download and compile Raylib if it's not already installed on your system.

## Playing

Start the game with `./build/TurnThem`.

- **Practice**: play against a bot on your own machine, no server needed.
- **Find Match**: connect to a server and get paired with the next player who is waiting.

Drag a card from the deck onto your half of the battlefield. Press Esc to leave a match.

- **Setup (20 seconds):** spend a separate allowance of 6 Supplies on starting
  artillery. Guns cost 3 each. Infantry is unavailable during setup; weapons
  remain idle and Supplies do not regenerate. Unspent setup allowance expires.
- **Combat (3 minutes):** both sides start with 5 live Supplies. Deploy guns or
  a **Rifle Squad** while your existing forces fight automatically.
- **Rifle Squad:** one card costs 3 Supplies and deploys three soldiers. Each
  advances toward the enemy HQ, stops within rifle range of an encountered
  enemy, attacks until it is defeated, then moves on. Soldiers have individual
  health and navigate around fixed emplacements. Each side can field up to 24
  living soldiers; a blocked formation or full unit limit rejects the whole card
  without spending Supplies.
- **Victory:** destroy the opposing 1,200-health main base. If both bases are
  destroyed on the same tick, or both survive the combat timer, the result is a
  draw. Leaving during setup cancels the match; leaving combat forfeits it.

Artillery needs a free cell with clearance from other emplacements; the base
area is reserved. Infantry does not permanently occupy grid cells. Friendly
forces do not damage one another. Placement zones stay fixed throughout combat.

The Cannon is a wheeled field gun; the SCannon is a heavier rounded swivel emplacement. Both use chunky olive armor, worn steel barrels, and brass fittings. A blue marker identifies friendly artillery and a red marker identifies enemies, with a subtle tint and matching health-bar outlines. Both cannon types start with 100 health and lose 35 health per enemy shell, so three hits destroy them. Health bars shrink from green to yellow to red, and damaged artillery darkens. Friendly shells do not damage your weapons.

Shots trigger barrel recoil, recovery, a brief muzzle flash, and fading smoke. The chassis stays the same size throughout the animation, and cards and placement previews use the resting pose. These effects follow the existing simulation ticks; fire rates, SCannon sweep angles, projectile speeds, and damage are unchanged.

Shells cross empty ground and hit the first enemy artillery, soldier, or base
along their path. Destroyed artillery stops firing and frees its placement
space. Shells already in flight keep moving after their firing weapon is lost.

During combat, gain 1 **Supply** every 2.8 seconds up to 10, and resupply twice
as fast in the last minute. The olive-and-steel deployment tray shows costs,
pending orders, and available Supplies. Base health replaces territory scoring.

Client options:

| Option | Meaning |
|---|---|
| `--server host[:port]` | Server to use for Find Match. Defaults to `$TURNTHEM_SERVER`, then `127.0.0.1:7777`. |
| `--practice-side white\|black` | Which side you play in practice. Black sees the board rotated. |
| `--sim-lag S`, `--sim-jitter S` | Add artificial network delay (in seconds), for testing. |

## Running a server

The server pairs players first come, first served, and runs every match itself, so neither player can cheat or gets a latency advantage. It uses one UDP port and doesn't need Raylib:

```bash
./build/TurnThemServer --port 7777
```

Options: `--match-seconds`, `--countdown-seconds` (setup duration, default 20),
`--max-players`. It logs one line when each match starts and ends, and shuts down
cleanly on Ctrl-C.

To try it locally, start the server, then play against the test bot (or open two game windows):

```bash
./build/TurnThemBot --server 127.0.0.1:7777 --count 1
```

`TurnThemBot --count N --matches M` also works as a load and soak test. It exits
non-zero if a client's battle checksum disagrees with the server. Checks cover
the grid, artillery health, main bases, and infantry state.

### On a Linux VPS

Build only the server; this needs no graphics libraries:

```bash
cmake -S . -B build -DTURNTHEM_BUILD_CLIENT=OFF -DTURNTHEM_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target TurnThemServer
```

Then:

1. Open UDP port 7777 in the firewall.
2. Install `deploy/turnthem-server.service` (edit the paths and user inside it).
3. Start the service:
   ```bash
   sudo cp deploy/turnthem-server.service /etc/systemd/system/
   sudo systemctl enable --now turnthem-server
   journalctl -u turnthem-server -f
   ```
4. Point players at it with `--server your.host.name` or `TURNTHEM_SERVER=your.host.name`.

Players need the same game version as the server. A mismatched client is told so when it connects.

## Artillery art

`art/artillery_source.png` is the generated three-column, two-row source sheet (rest, recoil, recovery; Cannon above SCannon). To repack its frames into the existing atlas, run `python3 tools/pack_artillery.py` with Pillow installed. The game uses the checked-in PNGs directly; Python and Pillow are not build dependencies. Keep the fixed frame canvases and pivot positions when replacing the artwork.
