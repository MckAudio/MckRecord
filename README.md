# MckRecord

## About

MckRecord is a small command line util I wrote to quickly record one shot samples from my drum machines.
The project uses [RtAudio](https://www.music.mcgill.ca/~gary/rtaudio/) and [libsndfile](http://www.mega-nerd.com/libsndfile/).

## Building

### Debian / Ubuntu
```
sudo apt install build-essential
sudo apt install librtaudio-dev libsndfile1-dev

make
```

### Fedora
```
sudo dnf install make automake libtool gcc-c++
sudo dnf install librtaudio-devel libsndfile-devel

make
```