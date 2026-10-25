# Tomo — User Guide

Tomo is a little virtual pet that lives on your desk. It hatches from an egg, grows up over a few days, and needs feeding, cleaning, playing with and putting to bed — just like a classic Tamagotchi, but with smooth animated graphics, real weather, and friends it can meet over Bluetooth.

Time is real: your pet gets hungry and sleepy on the real clock, keeps living while the device is unplugged, and catches up when you switch it back on.

## Supported devices

| Device | Screen | Motion sensor |
|---|---|---|
| Waveshare ESP32-C6-Touch-LCD-1.47 | 1.47" 172×320 | Yes — tilt and shake the pet |
| Guition JC3248W535EN (ESP32-S3) | 3.5" 320×480 | No — everything is done by touch |

Both run the same game; the bigger screen just shows everything larger.

---

## Setting up

### 1. Wi-Fi, location and time zone

Create `src/secrets.h` (it is never committed):

```c
#pragma once

#define WIFI_SSID "your-network"
#define WIFI_PASS "your-password"

// Where to get the weather from (decimal degrees, as strings).
#define WEATHER_LAT "51.5072"
#define WEATHER_LON "-0.1276"

// Your time zone as a POSIX TZ string. UK: "GMT0BST,M3.5.0/1,M10.5.0"
// Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"  US Eastern: "EST5EDT,M3.2.0,M11.1.0"
#define TZ_POSIX "GMT0BST,M3.5.0/1,M10.5.0"
```

Wi-Fi is only used for the clock and the weather. Without it the pet still works, but it treats every hour as midday and the scene shows clear skies.

### 2. Flashing

Install [PlatformIO](https://platformio.org/), plug the device in over USB-C, and check its COM port in `platformio.ini` (`upload_port`). Then:

```sh
# Windows only, once per terminal — otherwise the upload can hang or crash
$env:PYTHONIOENCODING="utf-8"; $env:PYTHONUTF8="1"

pio run -e esp32-c6-touch-lcd-147 -t upload   # 1.47" C6 board
pio run -e jc3248w535 -t upload               # 3.5" S3 board
```

Your pet is saved on the device itself, so re-flashing a new version keeps it.

---

## The home screen

```
┌──────────────────────────┐
│ 14:32     ● 42     ☁ 12° │  clock · coins · weather
│  (🍗)  (♥)  (⚡)  (💧)   │  hunger · happiness · energy · cleanliness
│      Tomo • Child Bun    │  name, life stage and form
│                          │
│         ☀   ☁            │  sky follows the real time of day and weather
│                          │
│          (pet)           │
│ ~~~~~~~~~~~~~~~~~~~~~~~~ │
│            ▔▔            │  swipe up here for the menu
└──────────────────────────┘
```

- **Stat rings** fill up when the pet is happy and drain over time. A ring **flashes red** when it drops below a quarter — that's your cue.
- **Toasts** pop up under the rings to tell you what's happening ("I'm hungry!", "Oops! Tap to clean", "Evolved into Kitty!").
- The **sky** changes from dawn to day to dusk to a starry night, and shows the real local weather: clouds, rain, snow, fog, or thunderstorms with lightning.

### Touching your pet

| Do this | What happens |
|---|---|
| **Stroke** the pet (drag your finger across it) | It loves it — hearts float up and happiness rises |
| **Tap** the pet | A quick pat and a squish |
| **Touch anywhere** and move your finger | The pet's eyes follow your finger |
| **Tap a poop** | Cleans up |
| **Swipe up** from the bottom, or tap the bottom edge | Opens the menu |
| Tap the egg | Sparkles — it wobbles more and more, and hatches on its own after 5 minutes |

### Moving the device (C6 board only)

| Do this | What happens |
|---|---|
| **Tilt** left or right | The pet leans and slides downhill |
| **Shake** | The pet gets dizzy. Shake it too often and it gets grumpy. Shaking a sleeping pet wakes it up — it won't thank you |
| **Pick it up** after it's been still | The pet looks surprised |
| **Lay it face down** at bedtime | Turns the lights off |

---

## The menu

Swipe up to open it; tap anywhere outside it to close.

| Button | What it does |
|---|---|
| **Feed** | Opens the food shop (see below) |
| **Play** | Choose a mini-game. Needs some energy, and not while asleep |
| **Clean** | Cleans up all poop and gives the pet a wash |
| **Light** | Turns the room light on or off |
| **Heal** | Medicine — cures sickness. Giving it to a healthy pet just upsets it |
| **Stats** | Detailed stats: all five bars, age, weight, care mistakes, pals |
| **Pals** | Friends your pet has met over Bluetooth |

### Food shop

Food costs coins. Earn coins by playing the mini-games.

| Food | Cost | Good for |
|---|---|---|
| Meal | 5 | Filling up (the best value for hunger) |
| Fruit | 6 | Hunger and a little health |
| Snack | 8 | A happiness boost |
| Cake | 20 | A big happiness boost — a treat, not a diet |

Don't keep feeding a pet that's already full: overfeeding a few times in a row makes it sick.

---

## Mini-games

Each game lasts **60 seconds**. You earn **half your score in coins (plus one)**, and the pet gets happier, a little tired, and slightly lighter.

**Treat Catch** — Treats fall from the sky; steer your pet to catch them. Drag your finger to move it (on the C6 you can also tilt the device).

| Item | Points |
|---|---|
| Apple | +1 |
| Candy | +2 |
| Gold star | +5 |
| Rock | −3 and a dizzy pet |

**Bubble Pop** — Tap bubbles before they float off the top. Keep popping without missing to build a **combo** for bonus points. **Gold bubbles** are worth 5.

---

## Looking after your pet

The four rings (plus a hidden **health** stat on the Stats screen) slowly drop on their own:

- **Hunger** — feed it.
- **Happiness** — stroke it, play games, give treats. Drops faster when the pet is hungry, dirty or sick.
- **Energy** — drops while awake, refills while sleeping. A very tired pet will take a nap on its own.
- **Cleanliness** — the pet poops every few hours. Each poop left lying around makes things dirtier.
- **Health** — falls if the pet is starving, sick, or living in a mess; recovers when it's well cared for.

### Bedtime

Pets sleep at night on the real clock:

| Stage | Bedtime | Wakes up |
|---|---|---|
| Baby | 20:00 | 08:00 |
| Child | 21:00 | 08:00 |
| Teen / Adult | 22:00 | 07:00 |

When it falls asleep, **turn the light off** (menu → Light). Leaving it on for half an hour counts as a care mistake and the pet sleeps badly. The light switches back on by itself in the morning.

### Sickness

A pet gets sick if two or more poops are left lying around for a couple of hours, if it goes hungry for too long, or if it's overfed. A sick pet looks green and sweaty. Use **Heal** from the menu to cure it.

### Care mistakes and growing up

Your pet grows through five stages:

| Stage | Lasts |
|---|---|
| Egg | 5 minutes |
| Baby | 1 hour |
| Child | 1 day |
| Teen | 3 days |
| Adult | for good |

Each time it grows up, its new **form** depends on how well it was looked after during the previous stage. A **care mistake** is leaving hunger or happiness at zero for 15 minutes, or leaving the light on at bedtime.

```
Baby Blob ─┬─ (good care) ──► Bun ────┬─ (good) ──► Kitty ──┬─ (great) ──► Drake
           │                          └─ (okay) ──► Mochi ──┼─ (great) ──► Star
           └─ (neglected) ─► Sprout ──┬─ (okay) ──► Mochi   ├─ (okay)  ──► Jelly
                                      └─ (poor) ──► Ghost ──┴─ (poor)  ──► Ghost
```

Drake is the rarest form — it needs near-perfect care all the way through.

### If you neglect it

A pet whose health stays at zero for half a day will **leave to go on an adventure**. A new egg appears in its place, and you keep your coins.

### While the device is off

Your pet keeps living. When you switch the device back on and it reconnects to Wi-Fi, it catches up on everything that happened while it was off — up to a week. A long holiday will leave you with a hungry, messy pet (or an empty nest), so give it a good feed before you go.

---

## Friends (Pals)

If another Tomo device is nearby, the two pets find each other over Bluetooth. Every few hours a friend **comes to visit**: it walks on screen, the two pets dance together, yours gets a happiness boost, and you receive a **gift of 10–30 coins**. Pets that have visited are listed under **Pals**, with how many times they've come by.

Visits only happen while your pet is awake, the home screen is showing, and the other device is within a few metres.

---

## Screen and power

- After **30 seconds** without a touch the screen dims; after **2 minutes** it turns off. The pet keeps living either way.
- Touch the screen (or on the C6, pick it up or tap it) to wake it. The first touch on a dark screen only wakes it — it won't press anything.
- While the screen is off, it **glows briefly** when your pet needs you: it's hungry, sick, has pooped, or something big happened.
- The clock and weather refresh about every 30 minutes, when you're not using the device (connecting to Wi-Fi briefly pauses the animation, so it waits until you've put it down).

---

## Troubleshooting

| Problem | Try |
|---|---|
| Clock shows `--:--` | The device hasn't reached the internet yet. Check `WIFI_SSID`/`WIFI_PASS` in `secrets.h`. |
| Weather never appears | Check `WEATHER_LAT`/`WEATHER_LON`, and that the network allows plain HTTP to `api.open-meteo.com`. |
| Night and day are at the wrong times | Fix `TZ_POSIX` in `secrets.h` for your time zone. |
| Pet won't eat or play | It's asleep, it's still an egg, it's too tired to play, or you need more coins. |
| No visits from a friend | Both pets need to be awake on their home screens and close together; each friend visits at most once every 6 hours. |
| Upload hangs or crashes on Windows | Set `PYTHONIOENCODING=utf-8` and `PYTHONUTF8=1` in the terminal first (see Flashing). |
| Wrong COM port | Run `pio device list` and update `upload_port` in `platformio.ini`. |
