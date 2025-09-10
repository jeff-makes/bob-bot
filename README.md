# Bob Bot

Bob Bot is a DIY air quality monitor and retro game console built with an ESP32, round TFT display, and a PM2.5 sensor.  
It measures fine particulate matter (PM2.5) indoors—useful for checking how much wildfire smoke makes it inside your house during wildfire season.  

When you’re not checking air quality, you can play two simple retro games (*falling blocks* and *brick breaker*) right on the display.  

👉 See Bob Bot in action on TikTok: [@jeff.makes](https://www.tiktok.com/@jeff.makes)

---

## Features
- Real-time PM2.5 monitoring with color-coded AQI labels  
- Animated boot splash + scanning sequence  
- AQ screen with subtle “breathing” number animation  
- Two built-in mini-games: *falling blocks* and *brick breaker*  
- Shared game-over screen with replay/back options  
- 3D-printed enclosure, tactile buttons, portable form factor  

---

## 🔧 Components & Wiring (as built)

| Component                 | Purpose      | ESP32 Pins / Wiring                                                                 | Voltage |
|---------------------------|--------------|-------------------------------------------------------------------------------------|---------|
| **1.28” Round TFT (GC9A01, SPI)** | Display      | VCC→3.3V, GND→GND, SCL→GPIO18, SDA/MOSI→GPIO23, RES→GPIO4, DC→GPIO2, CS→GPIO5      | 3.3V    |
| **PMS5003 PM2.5 sensor**  | Air sensor   | VCC→5V, GND→GND, TX→GPIO16 (ESP RX2), RX→GPIO17 (ESP TX2), SET→3.3V (always on)     | 5V      |
| **Buttons** (Red/Green/Blue) | Inputs     | Red→GPIO13, Green→GPIO12, Blue→GPIO14. Other leg of each → GND. `INPUT_PULLUP` active-LOW. | —       |

Notes:  
- PMS5003 cable was cut and adapted to female jumpers, with hot-glued strain relief.  
- Buttons are 12 mm tactiles (SunFounder kit or AliExpress).  

---

## 🧰 Libraries

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) (GC9A01 driver via SPI; configure pins in `User_Setup.h`)  
- [Adafruit_PM25AQI](https://github.com/adafruit/Adafruit_PM25AQI) (PMS5003 parsing)  
- *(Optional)* [Bounce2](https://github.com/thomasfredericks/Bounce2) — we use a simple timed debounce in code  

---

## 🖥️ Modes & Controls

### Air Quality Mode (default)
- Splash → scanning animation → PM2.5 reading with breathing number.  
- **GREEN** → enter Game Select.  

### Game Select
- **GREEN** → cycle between games.  
- **BLUE** → start game.  
- **RED** → back to AQ.  

### Falling Blocks
- **RED** → move left  
- **BLUE** → move right  
- **GREEN** → rotate  
- **RED+BLUE (together)** → hard drop  
- **Long-press RED (~2s)** → exit to Game Select  

### Brick Breaker
- **RED / BLUE** → paddle left/right (smoothed, 15% slower)  
- **GREEN** → launch ball (when stuck)  
- **Long-press RED (~2s)** → exit to Game Select  

### Game Over screen
- **BLUE** → Play Again  
- **RED (hold)** → Back  

---

## 🖼️ Display & UX
- 240×240 round TFT, bezel drawn to emphasize visible circle.  
- Calm AQ screen (no full-screen pulsing; just the number breathes).  
- Breakout uses night-style board with colorful bricks.  
