# bapple-lcd

"Bad Apple!!" displayed on a 20x4 character LCD at 7.5fps, using an ESP32-S3 microcontroller and an SD card.

### Video

(tbd)

### How it was made

Character LCDs have the capability of showing up to 8 different user-defined characters, which is a good number, though
it is obviously not enough to display a single 8 cols x 4 rows frame (this requires 32 custom characters!). However,
it is possible to create the illusion of more custom characters through a technique called multiplexing, which in this
case consists of loading 8 characters for a row, displaying the row, clearing the screen, then repeating this for all
4 rows. Because the LCD isn't capable of erasing everything in the screen immediately, and because the motion is too
fast for the human eye to notice such sudden changes, all of the rows appear to be displayed at the same time, creating
the illusion.
