# TWL-ROM-Optimize
Makes DSiWare ROMs slightly smaller

# Examples
- Crash-Course Domo - 2.17MB -> 1.83MB
- Dr. Mario Express - 3.71MB -> 3.13MB
- Glory Days: Tactical Defense - 5.91MB -> 5.05MB
- Mighty Flip Champs! - 5.90MB -> 5.37MB
- Mighty Milky Way - 10.8MB -> 10.2MB
- Shantae: Risky's Revenge - 15.9MB -> 15.0MB

# Compiling
`gcc source.c -o TWL-ROM-Optimize.exe`

# Preparation
1. Create a folder called `a7donors`
2. In `a7donors`, create a folder called `dsiware`
3. In `dsiware`:
     - Place a copy of either *Animal Crossing Clock* or *Mario Clock*, and rename to `sdk50.nds`
     - Place a copy of either *Bird & Beans* or *Paper Airplane Chase*, and rename to `sdk51.nds`
4. Again in `dsiware`, create a folder called `camerawifi`
5. In `camerawifi`:
     - Place a copy of *Nintendo DSi Browser* (Japan, Rev 0), and rename to `sdk50.nds`
     - Place a copy of *Nintendo DSi Browser* (Rev 1 or later), and rename to `sdk51.nds`
     - Place a copy of either *Bejeweled Twist* (DSiWare version) or *Photo Dojo*, and rename to `sdk53.nds`
     - Place a copy of either *Crazy Hamster* or *DS WiFi Settings*, and rename to `sdk55.nds`
6. Download [TinkeDSi](https://github.com/R-YaTian/TinkeDSi/releases)

# Usage
1. Place the ROMs in the same location as the .exe file.
2. In cmd, type `TWL-ROM-Optimize "romname.nds"`
     - More ROM names can be added for multiple optimization, as so: `TWL-ROM-Optimize "romname1.nds" "romname2.nds" "romname3.nds" ...`
     - You'll see an `out` folder created.
3. Go into the `out` folder, and then go into the folder with the ROM's name.
4. Drag and drop `base.nds` into TinkeDSi.
5. In TinkeDSi, open the `ftc` folder.
6. Replace `arm9.bin`, `arm7.bin`, and `arm7i.bin` with the ones created by TWL-ROM-Optimize for your ROM.
7. Finally, click `Save ROM`
