# mininet

```
sudo python3 ddil_simulation.py
```

```
h2 /root/dds_qos_project/build/DataSample subscriber > sub_test.log 2>&1 &
h1 /root/dds_qos_project/build/DataSample publisher > pub_test.log 2>&1 &
```
```
link h1 s1 down
```

```
link h1 s1 up
```
```
exit
```

# fast-dds
```
cat sub_test.log
```
