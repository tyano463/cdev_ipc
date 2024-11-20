# summary

This sample program demonstrates inter-process communication using character devices.

# build
```
make
```

# run

## device driver

```
sudo insmod driver/simple_fifo.ko
```

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

