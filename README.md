# otus

Lightweight system monitor built with **FTXUI**. Shows CPU/GPU/MEM/DISK, plus a grouped, hierarchical process view.

Please note GPU support is limited for nvidia GPUs.


## Clone & Build 
1.Clone 
```bash
git clone https://github.com/Sadish09/otus
cd otus 
```  
2.Build 
```bash
mkdir build && cd build 
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j 
./otus #opens the default dashboard 
```

## Usage Examples 

`otus` opens up the full dashboard - CPU/MEM/DISK guages and a GPU summary row and a live process tree sorted by CPU usage.

![otus_default](screenshots/otus_default)

`otus -cpu` prints overall CPU usage as a percentage, updating in place on one line. Useful for piping or scripting.

![ss for cpu](screenshots/otus_cpu)

`otus -mem` print used and total RAM in GiB.

![ss for mem](screenshots/otus_mem)

`otus -proc` prints the full hierarchical process tree grouped by parent/child relationships and sorted by CPU usage within each group.

![ss for proc](screenshots/otus_proc) 

`otus -proc -lim N(default 40)` limits number of processes printed out, can be changed in options.

![ss for proc lim](screenshots/otus_lim) 

