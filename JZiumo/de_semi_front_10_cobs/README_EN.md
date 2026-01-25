# Semi-Front 10-Cob Setup | Day Endless

[中文](./README.md) | English

[Return to Homepage](../README.md)

⚙️ [Script](./de_semi_front_10_cobs.cpp) · 💾 [Game Data](./game1_11.dat) · 🎥 [Bilibili](https://www.bilibili.com/video/BV1ivz2BpE5F/) | [YouTube](https://youtu.be/emwXFWFWcls)

## Setup Overview

<img src="./de_semi_front_10_cobs.png" width="400">

This setup primarily follows the `ch5` rhythm:

```
PP|IPP-PP|IPP-PP (601, 1438, 1438)
```

### Full 2-Flag Wave Plan

```
1  |2    |3  |4     |5  |6     |7     |8  |9     
PP |IPP-N|PP |IPP-PP|PP |IPP-PP|IPP-PP|PP |IPP-PP
601|1150 |601|1438  |601|1438  |1438  |601|NaN   

10 |11   |12 |13    |14 |15    |16    |17 |18    |19
PP |IPP-N|PP |IPP-PP|PP |IPP-PP|IPP-PP|PP |IPP-PP|IPP-PP
601|1150 |601|1438  |601|1438  |1438  |601|1438  |NaN
```
Based on the standard `ch5` rhythm, I use a doom (`IPP-N`) at `W2` and `W11`. The wave length `1150` is the maximum safe interval that protects the doom from the jack explosion. 

The purpose of using a doom here is to reduce ice usage, allowing the setup to run with **only one stored ice** at the start. However, this is not strictly necessary, since it's totally fine to start with more than one initially stored ice. Positions `(2,4)`, `(3,4)`, and `(4,4)` are all safe ice storage spots. 

### Choose Your Seeds / Zombies

<img src="./assets/choose_your_seeds.png" width="400">

```cpp
// zombies
ASetZombies({
    APOLE_VAULTING_ZOMBIE, 
    ADANCING_ZOMBIE, 
    AZOMBONI, 
    AJACK_IN_THE_BOX_ZOMBIE, 
    ADIGGER_ZOMBIE, 
    APOGO_ZOMBIE, 
    ABUNGEE_ZOMBIE, 
    ALADDER_ZOMBIE, 
    ACATAPULT_ZOMBIE, 
    AGARGANTUAR, 
    AGIGA_GARGANTUAR, 
    ABALLOON_ZOMBIE
});

// plants
ASelectCards({
    AICE_SHROOM, 
    AM_ICE_SHROOM, 
    ACOFFEE_BEAN, 
    ADOOM_SHROOM, 
    ASNOW_PEA, 
    ASQUASH, 
    APUFF_SHROOM, 
    ASUN_SHROOM, 
    ASCAREDY_SHROOM, 
    AFLOWER_POT
});
```
Snow Pea is used for **final cleanup** on `W9 / W19 / W20`, significantly reducing the pressure on giga blocking.

In practice, running with **only three fodder plants** is already stable.

## Script Implementation

📄 [Script File](./de_semi_front_10_cobs.cpp)

### Automatic Gargantuar Blocking

The `FodderManager` class implements automatic Gargantuar blocking. The implementation is heavily based on [vector-wlc’s classic 2-Cob script](https://github.com/vector-wlc/AsmVsZombies/blob/master/tutorial/scripts/jing_dian_2/jing_dian_2.cpp), adapted for this configuration. 

```cpp
class FodderManager
```

#### Public Interface

- `void startBlockGargantuar(int row, float column)`: Starts a `ATickRunner` thread that blocks gigas at the specified row and column.
- `void stopBlockGargantuar()`: Stops the Giga-blocking thread.

#### Fodder Selection

The `fodders` list defines all available fodder plants from the selected cards.
Plants with lower indices are prioritized (e.g. `APUFF_SHROOM` is placed first).

```cpp
std::vector<APlantType> fodders = {APUFF_SHROOM, ASUN_SHROOM, ASCAREDY_SHROOM, AFLOWER_POT, ASQUASH};
```

`ASQUASH` is intentionally placed at the end of the list. When there are too many gigas and no regular fodder is available, Squash will serve as a fallback fodder and instantly kill the gigas.

### Automatic Final-Wave Cleanup

The `FinalWaveHandler` class automates cleanup for `W9 / W19 / W20`.
Its main function is to determine which lane to stall, based on the distribution of live gigas.

```cpp
class FinalWaveHandler
```

The lifecycle methods include: 
- `start()`
- `stop()`
- `exit()`

`start()` and `stop()` are invoked inside wave-time connections, while `exit()` is called from the `AOnExitFight` state hook to safely handle cases where the player quits during thread execution.

#### Gigas Counting

`giga` is a dynamic list of alive gigas. When accessing it in a `AConnect` block, we can get real-time information about alive gigas on the field. 

```cpp
AAliveFilter<AZombie> giga;
```

The following logic counts gigas per lane:

```cpp
int giga_count[7] = {0};

void countGiga() {
    for (int i = 0; i < 7; ++i) {
        giga_count[i] = 0;
    }

    for (auto& zombie : giga) {
        if (zombie.AtWave() + 1 == wave) {
            int row = zombie.Row() + 1;
            giga_count[row] += 1;
        } 
    }
}
```

#### Lane Selection Logic

When `start()` is called:
1. The thread first counts the number of gigas per lane
2. Between **lane 1 and lane 5**, it selects the lane that:
    - Has at least one giga spawned in the current wave
    - Has **the smaller giga count**

If **neither lane 1 nor lane 5** has a giga in the current wave, we immediately activate the Maid cheat to preserve surviving Dancers.

```cpp
void start() {
    giga_runner.Start([this] {
        countGiga();

        int giga1 = giga_count[1];
        int giga5 = giga_count[5];

        if (giga1 == 0 && giga5 == 0) {
            dancer_cheat = true;
            AMaidCheats::Dancing();
            giga_runner.Stop();
            return;  
        } else if (giga1 > 0 && giga5 > 0) {
            target_row = (giga1 < giga5) ? 1 : 5;  
        } else if (giga1 > 0) {
            target_row = 1; 
        } else if (giga5 > 0) {
            target_row = 5; 
        }
        giga_runner.Stop();
    });
    // more operations based on "target_row" 
}
```

In the example below, `target_row` is set to 1. Obviously there is only one giga in lane 1, making it the safest lane to stall.

<img src="./assets/auto_fodder.png" width="400">

## Painter Utilities

The `Painter` class defines basic drawing utilities.
Its subclasses `WavePainter` and `CobHpPainter` are used to draw the **current wave number** and **Cob HP**, respectively.

The implementation is straightforward.

```cpp
class Painter
```

<img src="./assets/painters.png" width="400">