# AI-Implementation

Showcase of a full-scale bot AI system built using **procedural C-style code standards**.  
(No use of Classes, Inheritance, Virtual Functions, or Polymorphism.)

The code has undergone some cleanup, but remnants of legacy or outdated logic may still exist.

---


## 📺 YouTube Showcase Video
Watch it here: [YouTube link](https://www.youtube.com/watch?v=QfXUkHWNXbE)

---

## 🎯 Goal

Develop core systems that enable bots to exhibit **emergent behavior**, **fluid movement**, and robust interaction with the game world.

---

## 🚀 Features

### 🧠 AI Core
- **Finite State Machine (FSM)** architecture for clean, predictable behavior switching.
- Macro-based bot definition system:
  - Each bot type is isolated in its own `.cpp` file.
  - Promotes separation of logic and modular behavior design.
- Data-driven logic — no object-oriented dependencies.

---

### 🦿 Movement
- Velocity-based steering behaviors.
- **Crowd avoidance steering** (dynamic local obstacle avoidance).
- **NavMesh navigation** for ground units.
- **3D grid-based pathfinding** for flying units.
- Supports **root motion animation-based movement**.

---

### 🎯 Targeting System
- Weighted target evaluation using configurable **criteria**.
- **Line-of-sight** visibility checks.

---

### 🧊 Status Effects (Debuffs)
- **Movement modifiers** (e.g., slow, speed boost).
- **State overrides** (e.g., stun, freeze).
- Interaction states for **pickup-able bots**.

---

### 🧩 Interaction
- Modular **interactable joints** for procedural interaction.
