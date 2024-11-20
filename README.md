# summary

This sample program demonstrates inter-process communication using character devices.

# build
```
make
insmod driver/simple_fifo.ko
```

# run

## rx
```
sudo -i
./rx
```

## tx
```
sudo -i
./tx
```

# checked environment

- fedora-40 + xfce

